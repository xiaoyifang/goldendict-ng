// https://bitbucket.org/xwang/mdict-analysis
// https://github.com/zhansliu/writemdict/blob/master/fileformat.md
// Octopus MDict Dictionary File (.mdx) and Resource File (.mdd) Analyser
//
// Copyright (C) 2012, 2013 Xiaoqiang Wang <xiaoqiangwang AT gmail DOT com>
// Copyright (C) 2013 Timon Wong <timon86.wang AT gmail DOT com>
// Copyright (C) 2015 Zhe Wang <0x1998 AT gmail DOT com>
//
// This program is a free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// You can get a copy of GNU General Public License along this program
// But you can always get it from http://www.gnu.org/licenses/gpl.txt
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

#include "mdictparser.hh"

#include "decompress.hh"
#include "htmlescape.hh"
#include "iconv.hh"
#include "ripemd.hh"
#include "utils.hh"
#include <QByteArray>
#include <QDataStream>
#include <QDomDocument>
#include <QStringList>
#include <QTextDocumentFragment>
#include <QtEndian>
#include <lzo/lzo1x.h>
#include <zlib.h>

namespace Mdict {

enum EncryptedSection {
  EcryptedHeadWordHeader = 1,
  EcryptedHeadWordIndex  = 2
};

// V3 compression methods
enum V3CompressionMethod : uint8_t {
  V3CompNone    = 0,
  V3CompLzo     = 1,
  V3CompDeflate = 2,
  V3CompLzma    = 3,
  V3CompBzip2   = 4,
  V3CompLz4     = 5
};

// V3 encryption methods
enum V3EncryptionMethod : uint8_t {
  V3EncNone    = 0,
  V3EncSimple  = 1,
  V3EncSalsa20 = 2
};

static inline int u16StrSize( const ushort * unicode )
{
  int size = 0;
  if ( unicode ) {
    while ( unicode[ size ] != 0 ) {
      size++;
    }
  }
  return size;
}

static QDomNamedNodeMap parseHeaderAttributes( const QString & headerText )
{
  QDomNamedNodeMap attributes;
  QDomDocument doc;
  doc.setContent( headerText );

  QDomElement docElem = doc.documentElement();
  attributes          = docElem.attributes();


  return attributes;
}

// ============================================================================
// Salsa20 implementation for V3 decryption
// ============================================================================

namespace {

struct Salsa20Context {
  uint32_t input[16];
};

inline uint32_t rotl32( uint32_t x, int n )
{
  return ( x << n ) | ( x >> ( 32 - n ) );
}

void salsa20Core( uint8_t output[64], const uint32_t input[16] )
{
  uint32_t x[16];
  memcpy( x, input, 64 );

  for ( int i = 0; i < 10; ++i ) {
    // Column round
    x[ 4] ^= rotl32( x[ 0] + x[12],  7 );
    x[ 8] ^= rotl32( x[ 4] + x[ 0],  9 );
    x[12] ^= rotl32( x[ 8] + x[ 4], 13 );
    x[ 0] ^= rotl32( x[12] + x[ 8], 18 );
    x[ 9] ^= rotl32( x[ 5] + x[ 1],  7 );
    x[13] ^= rotl32( x[ 9] + x[ 5],  9 );
    x[ 1] ^= rotl32( x[13] + x[ 9], 13 );
    x[ 5] ^= rotl32( x[ 1] + x[13], 18 );
    x[14] ^= rotl32( x[10] + x[ 6],  7 );
    x[ 2] ^= rotl32( x[14] + x[10],  9 );
    x[ 6] ^= rotl32( x[ 2] + x[14], 13 );
    x[10] ^= rotl32( x[ 6] + x[ 2], 18 );
    x[ 3] ^= rotl32( x[15] + x[11],  7 );
    x[ 7] ^= rotl32( x[ 3] + x[15],  9 );
    x[11] ^= rotl32( x[ 7] + x[ 3], 13 );
    x[15] ^= rotl32( x[11] + x[ 7], 18 );

    // Row round
    x[ 1] ^= rotl32( x[ 0] + x[ 3],  7 );
    x[ 2] ^= rotl32( x[ 1] + x[ 0],  9 );
    x[ 3] ^= rotl32( x[ 2] + x[ 1], 13 );
    x[ 0] ^= rotl32( x[ 3] + x[ 2], 18 );
    x[ 6] ^= rotl32( x[ 5] + x[ 4],  7 );
    x[ 7] ^= rotl32( x[ 6] + x[ 5],  9 );
    x[ 4] ^= rotl32( x[ 7] + x[ 6], 13 );
    x[ 5] ^= rotl32( x[ 4] + x[ 7], 18 );
    x[11] ^= rotl32( x[10] + x[ 9],  7 );
    x[ 8] ^= rotl32( x[11] + x[10],  9 );
    x[ 9] ^= rotl32( x[ 8] + x[11], 13 );
    x[10] ^= rotl32( x[ 9] + x[ 8], 18 );
    x[12] ^= rotl32( x[15] + x[14],  7 );
    x[13] ^= rotl32( x[12] + x[15],  9 );
    x[14] ^= rotl32( x[13] + x[12], 13 );
    x[15] ^= rotl32( x[14] + x[13], 18 );
  }

  for ( int i = 0; i < 16; ++i ) {
    x[i] += input[i];
    output[i*4 + 0] = (uint8_t)( x[i] );
    output[i*4 + 1] = (uint8_t)( x[i] >> 8 );
    output[i*4 + 2] = (uint8_t)( x[i] >> 16 );
    output[i*4 + 3] = (uint8_t)( x[i] >> 24 );
  }
}

void salsa20KeySetup( Salsa20Context * ctx, const uint8_t * key, int keyBits )
{
  static const char sigma[16] = "expand 32-byte k";
  static const char tau[16]   = "expand 16-byte k";
  const char * constants = ( keyBits == 256 ) ? sigma : tau;

  ctx->input[0]  = qFromLittleEndian<uint32_t>( (const uchar *)constants );
  ctx->input[5]  = qFromLittleEndian<uint32_t>( (const uchar *)constants + 4 );
  ctx->input[10] = qFromLittleEndian<uint32_t>( (const uchar *)constants + 8 );
  ctx->input[15] = qFromLittleEndian<uint32_t>( (const uchar *)constants + 12 );

  ctx->input[1] = qFromLittleEndian<uint32_t>( key );
  ctx->input[2] = qFromLittleEndian<uint32_t>( key + 4 );
  ctx->input[3] = qFromLittleEndian<uint32_t>( key + 8 );
  ctx->input[4] = qFromLittleEndian<uint32_t>( key + 12 );

  if ( keyBits == 256 ) {
    key += 16;
  }

  ctx->input[11] = qFromLittleEndian<uint32_t>( key );
  ctx->input[12] = qFromLittleEndian<uint32_t>( key + 4 );
  ctx->input[13] = qFromLittleEndian<uint32_t>( key + 8 );
  ctx->input[14] = qFromLittleEndian<uint32_t>( key + 12 );
}

void salsa20IvSetup( Salsa20Context * ctx, const uint8_t * iv )
{
  ctx->input[6] = qFromLittleEndian<uint32_t>( iv );
  ctx->input[7] = qFromLittleEndian<uint32_t>( iv + 4 );
  ctx->input[8] = 0;
  ctx->input[9] = 0;
}

void salsa20Decrypt( const uint8_t * input, uint8_t * output, size_t len, const uint8_t * key )
{
  Salsa20Context ctx;
  uint8_t nonce[8] = {0};
  salsa20KeySetup( &ctx, key, 128 );
  salsa20IvSetup( &ctx, nonce );

  uint8_t block[64];
  while ( len > 0 ) {
    salsa20Core( block, ctx.input );
    ctx.input[8]++;
    if ( ctx.input[8] == 0 ) {
      ctx.input[9]++;
    }

    size_t blockLen = ( len < 64 ) ? len : 64;
    for ( size_t i = 0; i < blockLen; ++i ) {
      output[i] = input[i] ^ block[i];
    }

    input += blockLen;
    output += blockLen;
    len -= blockLen;
  }
}

// Simple XOR decryption for V3
void simpleDecryptV3( uint8_t * buffer, int len, const uint8_t * key, int keyLen )
{
  if ( keyLen == 0 ) return;

  uint8_t lastByte = 0x36;
  for ( int i = 0; i < len; ++i ) {
    uint8_t b = buffer[i];
    buffer[i] = ( ( ( b & 0x0f ) << 4 ) | ( ( b & 0xf0 ) >> 4 ) ) ^
                key[i % keyLen] ^ (uint8_t)( i & 0xFF ) ^ lastByte;
    lastByte = b;
  }
}

// Fast hash for V3 crypto key generation from UUID
QByteArray fastHashDigest( const QByteArray & data )
{
  QByteArray result( 16, 0 );
  uint32_t hash = 0;
  for ( int i = 0; i < data.size(); ++i ) {
    hash = hash * 31 + (uint8_t)data[i];
  }
  for ( int i = 0; i < 4; ++i ) {
    result[i] = (uint8_t)( hash >> ( i * 8 ) );
    result[i+4] = (uint8_t)( hash >> ( i * 8 ) );
    result[i+8] = (uint8_t)( hash >> ( i * 8 ) );
    result[i+12] = (uint8_t)( hash >> ( i * 8 ) );
  }
  return result;
}

// RIPEMD-128 digest wrapper
QByteArray ripemdDigest( const QByteArray & data )
{
  RIPEMD128 ripemd;
  ripemd.update( (const uchar *)data.constData(), data.size() );
  QByteArray digest( 16, 0 );
  ripemd.digest( (uchar *)digest.data() );
  return digest;
}

} // anonymous namespace

size_t MdictParser::RecordIndex::bsearch( const vector< MdictParser::RecordIndex > & offsets, qint64 val )
{
  if ( offsets.size() == 0 ) {
    return (size_t)( -1 );
  }

  auto it = std::lower_bound( offsets.begin(), offsets.end(), val );
  if ( it != offsets.end() && *it == val ) {
    return std::distance( offsets.begin(), it );
  }

  return (size_t)( -1 );
}

MdictParser::MdictParser():
  version_( 0 ),
  numHeadWordBlocks_( 0 ),
  headWordBlockInfoSize_( 0 ),
  headWordBlockSize_( 0 ),
  headWordBlockInfoPos_( 0 ),
  headWordPos_( 0 ),
  totalRecordsSize_( 0 ),
  recordPos_( 0 ),
  wordCount_( 0 ),
  numberTypeSize_( 0 ),
  encrypted_( 0 ),
  rtl_( false ),
  isUtf16Encoding_( false ),
  contentDataOffset_( 0 ),
  contentDataSize_( 0 ),
  contentBlockCount_( 0 ),
  keyDataOffset_( 0 ),
  keyDataSize_( 0 ),
  totalContentDecompressedSize_( 0 ),
  currentKeyBlockIndex_( 0 ),
  currentEntryNo_( 0 )
{
}

MdictParser::~MdictParser()
{
  if ( file_ && file_->isOpen() ) {
    file_->close();
  }
}

bool MdictParser::open( const char * filename )
{
  filename_ = QString::fromUtf8( filename );
  file_     = new QFile( filename_ );

  qDebug( "MdictParser: open %s", filename );

  if ( file_.isNull() || !file_->exists() ) {
    return false;
  }

  if ( !file_->open( QIODevice::ReadOnly ) ) {
    return false;
  }

  QDataStream in( file_ );
  in.setByteOrder( QDataStream::BigEndian );

  if ( !readHeader( in ) ) {
    return false;
  }

  // Branch based on format version
  if ( isV3() ) {
    qDebug( "MdictParser: Detected v3 (ZDB) format" );
    return openV3();
  }

  // V1/V2 format
  if ( !readHeadWordBlockInfos( in ) ) {
    return false;
  }

  if ( !readRecordBlockInfos() ) {
    return false;
  }

  return true;
}

bool MdictParser::readNextHeadWordIndex( MdictParser::HeadWordIndex & headWordIndex )
{
  // V3 uses different iteration logic
  if ( isV3() ) {
    return readNextHeadWordIndexV3( headWordIndex );
  }

  // V1/V2 logic
  if ( headWordBlockInfosIter_ == headWordBlockInfos_.end() ) {
    return false;
  }

  qint64 compressedSize   = headWordBlockInfosIter_->first;
  qint64 decompressedSize = headWordBlockInfosIter_->second;

  if ( compressedSize < 8 ) {
    return false;
  }

  ScopedMemMap compressed( *file_, headWordPos_, compressedSize );
  if ( !compressed.startAddress() ) {
    return false;
  }

  headWordPos_ += compressedSize;
  QByteArray decompressed;
  if ( !parseCompressedBlock( compressedSize, (char *)compressed.startAddress(), decompressedSize, decompressed ) ) {
    return false;
  }

  headWordIndex = splitHeadWordBlock( decompressed );
  ++headWordBlockInfosIter_;
  return true;
}

bool MdictParser::checkAdler32( const char * buffer, unsigned int len, quint32 checksum )
{
  uLong adler = adler32( 0L, Z_NULL, 0 );
  adler       = adler32( adler, (const Bytef *)buffer, len );
  return ( adler & 0xFFFFFFFF ) == checksum;
}

QString MdictParser::toUtf16( const char * fromCode, const char * from, size_t fromSize )
{
  if ( !fromCode || !from ) {
    return QString();
  }

  return Iconv::toQString( fromCode, from, fromSize );
}

bool MdictParser::decryptHeadWordIndex( char * buffer, qint64 len )
{
  RIPEMD128 ripemd;
  ripemd.update( (const uchar *)buffer + 4, 4 );
  ripemd.update( (const uchar *)"\x95\x36\x00\x00", 4 );

  uint8_t key[ 16 ];
  ripemd.digest( key );

  buffer += 8;
  len -= 8;
  uint8_t prev = 0x36;
  for ( qint64 i = 0; i < len; ++i ) {
    uint8_t byte = buffer[ i ];
    byte         = ( byte >> 4 ) | ( byte << 4 );
    byte         = byte ^ prev ^ ( i & 0xFF ) ^ key[ i % 16 ];
    prev         = buffer[ i ];
    buffer[ i ]  = byte;
  }
  return true;
}

bool MdictParser::parseCompressedBlock( qint64 compressedBlockSize,
                                        const char * compressedBlockPtr,
                                        qint64 decompressedBlockSize,
                                        QByteArray & decompressedBlock )
{
  if ( compressedBlockSize <= 8 ) {
    return false;
  }

  // compression type
  quint32 type     = qFromBigEndian< quint32 >( (const uchar *)compressedBlockPtr );
  quint32 checksum = qFromBigEndian< quint32 >( (const uchar *)compressedBlockPtr + 4 );
  const char * buf = compressedBlockPtr + 8;
  qint64 size      = compressedBlockSize - 8;

  switch ( type ) {
    case 0x00000000:
      // No compression
      if ( !checkAdler32( buf, size, checksum ) ) {
        qWarning( "MDict: parseCompressedBlock: plain: checksum not match" );
        return false;
      }

      decompressedBlock = QByteArray( buf, size );
      return true;

    case 0x01000000: {
      // LZO compression
      int result;
      lzo_uint blockSize = (lzo_uint)decompressedBlockSize;
      decompressedBlock.resize( blockSize );
      result = lzo1x_decompress_safe( (const uchar *)buf, size, (uchar *)decompressedBlock.data(), &blockSize, NULL );

      if ( result != LZO_E_OK || blockSize != (lzo_uint)decompressedBlockSize ) {
        qWarning( "MDict: parseCompressedBlock: decompression failed" );
        return false;
      }

      if ( checksum
           != lzo_adler32( lzo_adler32( 0, NULL, 0 ), (const uchar *)decompressedBlock.constData(), blockSize ) ) {
        qWarning( "MDict: parseCompressedBlock: lzo: checksum does not match" );
        return false;
      }
    } break;

    case 0x02000000:
      // zlib compression
      decompressedBlock = zlibDecompress( buf, size, checksum );
      if ( decompressedBlock.isEmpty() ) {
        qWarning( "MDict: parseCompressedBlock: zlib: failed to decompress or checksum does not match" );
        return false;
      }
      break;
    default:
      qWarning( "MDict: parseCompressedBlock: unknown type" );
      return false;
  }

  return true;
}

qint64 MdictParser::readNumber( QDataStream & in )
{
  if ( numberTypeSize_ == 8 ) {
    qint64 val;
    in >> val;
    return val;
  }
  else {
    quint32 val;
    in >> val;
    return val;
  }
}

quint32 MdictParser::readU8OrU16( QDataStream & in, bool isU16 )
{
  if ( isU16 ) {
    quint16 val;
    in >> val;
    return val;
  }
  else {
    quint8 val;
    in >> val;
    return val;
  }
}

bool MdictParser::readHeader( QDataStream & in )
{
  qint32 headerTextSize;
  in >> headerTextSize;

  // Sanity check
  if ( headerTextSize <= 0 || headerTextSize > 1024 * 1024 ) {
    qWarning( "MDict: readHeader: invalid header size %d", headerTextSize );
    return false;
  }

  QByteArray headerData = file_->read( headerTextSize );
  if ( headerData.size() != headerTextSize ) {
    return false;
  }

  // Detect encoding: V3 uses UTF-8, V1/V2 uses UTF-16LE
  // UTF-16LE starts with '<' (0x3C) followed by 0x00
  QString headerText;
  bool isHeaderUtf16 = ( headerTextSize > 1 && headerData[0] == '<' && headerData[1] == 0 );

  if ( isHeaderUtf16 ) {
    headerText = toUtf16( "UTF-16LE", headerData.constData(), headerTextSize );
    // V1/V2: CRC is little-endian
    in.setByteOrder( QDataStream::LittleEndian );
  }
  else {
    headerText = QString::fromUtf8( headerData );
    // V3: CRC is big-endian, keep BigEndian
  }

  // Read and verify Adler-32 checksum
  quint32 checksum;
  in >> checksum;
  
  // For v3 (big-endian CRC), we need to compare directly
  // For v1/v2 (little-endian CRC), checkAdler32 works as-is
  if ( !checkAdler32( headerData.constData(), headerTextSize, checksum ) ) {
    qWarning( "MDict: readHeader: checksum does not match" );
    return false;
  }
  
  // Reset to BigEndian for subsequent reads
  in.setByteOrder( QDataStream::BigEndian );

  // Parse stylesheet
  QString styleSheets;
  if ( headerText.contains( "StyleSheet" ) ) {
    const QRegularExpression rx( "StyleSheet=\"([^\"]*?)\"", QRegularExpression::CaseInsensitiveOption );
    auto match = rx.match( headerText );
    if ( match.hasMatch() || match.hasPartialMatch() ) {
      styleSheets = match.captured( 1 );
    }
  }

  // Remove control characters that break Qt XML parsing
  headerText.remove( QRegularExpression( "\\p{C}", QRegularExpression::UseUnicodePropertiesOption ) );

  QDomNamedNodeMap headerAttributes = parseHeaderAttributes( headerText );
  if ( headerAttributes.isEmpty() ) {
    return false;
  }

  // Determine version - check both GeneratedByEngineVersion and RequiredEngineVersion
  QString versionStr = headerAttributes.namedItem( "GeneratedByEngineVersion" ).toAttr().value();
  if ( versionStr.isEmpty() ) {
    versionStr = headerAttributes.namedItem( "RequiredEngineVersion" ).toAttr().value();
  }
  version_ = versionStr.toDouble();

  // Also check for ZDB tag which indicates v3
  QDomDocument doc;
  doc.setContent( headerText );
  QString rootTag = doc.documentElement().tagName().toLower();
  if ( rootTag == "zdb" || rootTag == "library_data" ) {
    if ( version_ < 3.0 ) {
      version_ = 3.0;  // Force v3 for ZDB format
    }
  }

  // Set number type size based on version
  if ( version_ < 2.0 ) {
    numberTypeSize_ = 4;
  }
  else {
    numberTypeSize_ = 8;
  }

  // Parse encoding
  encoding_ = headerAttributes.namedItem( "Encoding" ).toAttr().value();
  if ( encoding_.isEmpty() ) {
    encoding_ = isV3() ? "UTF-8" : "UTF-16LE";
  }
  else if ( encoding_ == "GBK" || encoding_ == "GB2312" ) {
    encoding_ = "GB18030";
  }
  else if ( encoding_ == "UTF-16" ) {
    encoding_ = "UTF-16LE";
  }
  isUtf16Encoding_ = encoding_.startsWith( "UTF-16", Qt::CaseInsensitive );

  // Parse stylesheet entries
  if ( !styleSheets.isEmpty() ) {
    QStringList lines = styleSheets.split( QRegularExpression( "[\r\n]" ), Qt::KeepEmptyParts );
    for ( int i = 0; i < lines.size() - 3; i += 3 ) {
      styleSheets_[ lines[ i ].toInt() ] =
        pair( Html::fromHtmlEscaped( lines[ i + 1 ] ), Html::fromHtmlEscaped( lines[ i + 2 ] ) );
    }
  }

  // Parse encryption type
  encrypted_ = headerAttributes.namedItem( "Encrypted" ).toAttr().value().toInt();

  // Parse metadata
  rtl_ = headerAttributes.namedItem( "Left2Right" ).toAttr().value() != "Yes";
  
  QString title = headerAttributes.namedItem( "Title" ).toAttr().value();
  if ( title.isEmpty() || title == "Title (No HTML code allowed)" ) {
    QFileInfo fi( filename_ );
    title_ = fi.baseName();
  }
  else {
    if ( title.contains( '<' ) || title.contains( '>' ) ) {
      title_ = QTextDocumentFragment::fromHtml( title ).toPlainText();
    }
    else {
      title_ = title;
    }
  }

  description_ = headerAttributes.namedItem( "Description" ).toAttr().value();

  // V3 specific: UUID for crypto key generation
  uuid_ = headerAttributes.namedItem( "UUID" ).toAttr().value();

  // Generate crypto key for V3
  if ( isV3() && !uuid_.isEmpty() ) {
    cryptoKey_ = fastHashDigest( uuid_.toUtf8() );
  }

  return true;
}

bool MdictParser::readHeadWordBlockInfos( QDataStream & in )
{
  QByteArray header = file_->read( version_ >= 2.0 ? ( numberTypeSize_ * 5 ) : ( numberTypeSize_ * 4 ) );
  QDataStream stream( header );

  // number of headword blocks
  numHeadWordBlocks_ = readNumber( stream );
  // number of entries
  wordCount_ = readNumber( stream );

  // number of bytes of a headword block info after decompression
  qint64 decompressedSize;
  if ( version_ >= 2.0 ) {
    stream >> decompressedSize;
  }

  // number of bytes of a headword block info before decompression
  headWordBlockInfoSize_ = readNumber( stream );
  // number of bytes of a headword block
  headWordBlockSize_ = readNumber( stream );

  // Adler-32 checksum of the header. If those are encrypted, it is
  // the checksum of the decrypted version
  if ( version_ >= 2.0 ) {
    quint32 checksum;
    in >> checksum;
    if ( !checkAdler32( header.constData(), numberTypeSize_ * 5, checksum ) ) {
      return false;
    }
  }

  headWordBlockInfoPos_ = file_->pos();

  // read headword block info
  QByteArray headWordBlockInfo = file_->read( headWordBlockInfoSize_ );
  if ( headWordBlockInfo.size() != headWordBlockInfoSize_ ) {
    return false;
  }

  if ( version_ >= 2.0 ) {
    // decrypt
    if ( encrypted_ & EcryptedHeadWordIndex ) {
      if ( !decryptHeadWordIndex( headWordBlockInfo.data(), headWordBlockInfo.size() ) ) {
        return false;
      }
    }

    QByteArray decompressed;
    if ( !parseCompressedBlock( headWordBlockInfo.size(), headWordBlockInfo.data(), decompressedSize, decompressed ) ) {
      return false;
    }

    headWordBlockInfos_ = decodeHeadWordBlockInfo( decompressed );
  }
  else {
    headWordBlockInfos_ = decodeHeadWordBlockInfo( headWordBlockInfo );
  }

  headWordPos_            = file_->pos();
  headWordBlockInfosIter_ = headWordBlockInfos_.begin();
  return true;
}

bool MdictParser::readRecordBlockInfos()
{
  file_->seek( headWordBlockInfoPos_ + headWordBlockInfoSize_ + headWordBlockSize_ );

  QDataStream in( file_ );
  in.setByteOrder( QDataStream::BigEndian );
  qint64 numRecordBlocks = readNumber( in );
  readNumber( in ); // total number of records, skip
  qint64 recordInfoSize = readNumber( in );
  totalRecordsSize_     = readNumber( in );
  recordPos_            = file_->pos() + recordInfoSize;

  // Build record block index
  recordBlockInfos_.reserve( numRecordBlocks );

  qint64 acc1 = 0;
  qint64 acc2 = 0;
  for ( qint64 i = 0; i < numRecordBlocks; i++ ) {
    RecordIndex r;
    r.compressedSize   = readNumber( in );
    r.decompressedSize = readNumber( in );
    r.startPos         = acc1;
    r.endPos           = acc1 + r.compressedSize;
    r.shadowStartPos   = acc2;
    r.shadowEndPos     = acc2 + r.decompressedSize;
    recordBlockInfos_.push_back( r );

    acc1 = r.endPos;
    acc2 = r.shadowEndPos;
  }

  return true;
}

MdictParser::BlockInfoVector MdictParser::decodeHeadWordBlockInfo( const QByteArray & headWordBlockInfo )
{
  BlockInfoVector headWordBlockInfos;

  QDataStream s( headWordBlockInfo );
  s.setByteOrder( QDataStream::BigEndian );

  bool isU16       = false;
  int textTermSize = 0;

  if ( version_ >= 2.0 ) {
    isU16        = true;
    textTermSize = 1;
  }

  while ( !s.atEnd() ) {
    // Number of keywords in the block
    s.skipRawData( numberTypeSize_ );
    // Size of the first headword in the block
    quint32 textHeadSize = readU8OrU16( s, isU16 );
    // The first headword
    if ( encoding_ != "UTF-16LE" ) {
      s.skipRawData( textHeadSize + textTermSize );
    }
    else {
      s.skipRawData( ( textHeadSize + textTermSize ) * 2 );
    }
    // Size of the last headword in the block
    quint32 textTailSize = readU8OrU16( s, isU16 );
    // The last headword
    if ( encoding_ != "UTF-16LE" ) {
      s.skipRawData( textTailSize + textTermSize );
    }
    else {
      s.skipRawData( ( textTailSize + textTermSize ) * 2 );
    }

    // headword block compressed size
    qint64 compressedSize = readNumber( s );
    // headword block decompressed size
    qint64 decompressedSize = readNumber( s );
    headWordBlockInfos.emplace_back( compressedSize, decompressedSize );
  }

  return headWordBlockInfos;
}

MdictParser::HeadWordIndex MdictParser::splitHeadWordBlock( const QByteArray & block )
{
  HeadWordIndex index;

  const char * p   = block.constData();
  const char * end = p + block.size();

  while ( p < end ) {
    qint64 headWordId = ( numberTypeSize_ == 8 ) ? qFromBigEndian< qint64 >( (const uchar *)p ) :
                                                   qFromBigEndian< quint32 >( (const uchar *)p );
    p += numberTypeSize_;
    QByteArray headWordBuf;

    if ( encoding_ == "UTF-16LE" ) {
      int headWordLength = u16StrSize( (const ushort *)p );
      headWordBuf        = QByteArray( p, ( headWordLength + 1 ) * 2 );
    }
    else {
      int headWordLength = strlen( p );
      headWordBuf        = QByteArray( p, headWordLength + 1 );
    }
    p += headWordBuf.size();
    QString headWord = toUtf16( encoding_, headWordBuf.constBegin(), headWordBuf.size() );
    index.emplace_back( headWordId, headWord );
  }

  return index;
}

bool MdictParser::readRecordBlock( MdictParser::HeadWordIndex & headWordIndex,
                                   MdictParser::RecordHandler & recordHandler )
{
  // V3 uses different record block handling
  if ( isV3() ) {
    return readRecordBlockV3( headWordIndex, recordHandler );
  }

  // V1/V2 logic
  // cache the index, the headWordIndex is already sorted
  size_t idx = 0;

  for ( HeadWordIndex::const_iterator i = headWordIndex.begin(); i != headWordIndex.end(); ++i ) {
    if ( recordBlockInfos_[ idx ].shadowEndPos <= i->first ) {
      idx = RecordIndex::bsearch( recordBlockInfos_, i->first );
    }

    if ( idx == (size_t)( -1 ) ) {
      return false;
    }

    const RecordIndex & recordIndex     = recordBlockInfos_[ idx ];
    HeadWordIndex::const_iterator iNext = i + 1;
    qint64 recordSize;
    if ( iNext == headWordIndex.end() ) {
      recordSize = recordIndex.shadowEndPos - i->first;
    }
    else {
      recordSize = iNext->first - i->first;
    }

    RecordInfo recordInfo;
    recordInfo.compressedBlockPos    = recordPos_ + recordIndex.startPos;
    recordInfo.recordOffset          = i->first - recordIndex.shadowStartPos;
    recordInfo.decompressedBlockSize = recordIndex.decompressedSize;
    recordInfo.compressedBlockSize   = recordIndex.compressedSize;
    recordInfo.recordSize            = recordSize;

    recordHandler.handleRecord( i->second, recordInfo );
  }

  return true;
}

QString & MdictParser::substituteStylesheet( QString & article, const MdictParser::StyleSheets & styleSheets )
{
  QRegularExpression rx( "`(\\d+)`", QRegularExpression::UseUnicodePropertiesOption );
  QString articleNewText;

  QString endStyle;
  int pos = 0;

  QRegularExpressionMatchIterator it = rx.globalMatch( article );
  while ( it.hasNext() ) {
    QRegularExpressionMatch match = it.next();
    int styleId                   = match.captured( 1 ).toInt();
    articleNewText += article.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    StyleSheets::const_iterator iter = styleSheets.find( styleId );

    if ( iter != styleSheets.end() ) {
      QString rep = endStyle + iter->second.first;
      articleNewText += rep;

      endStyle = iter->second.second;
    }
    else {
      articleNewText += endStyle;

      endStyle = "";
    }
  }
  if ( pos ) {
    articleNewText += Utils::rstripnull( article.mid( pos ) );
    article = articleNewText;
    articleNewText.clear();
  }
  article += endStyle;
  return article;
}

// ============================================================================
// V3 Format Implementation
// ============================================================================

bool MdictParser::parseCompressedBlockV3( const QByteArray & blockData,
                                          const QByteArray & cryptoKey,
                                          quint32 decompressedSize,
                                          QByteArray & decompressedBlock )
{
  if ( blockData.size() < 8 ) {
    qWarning( "MDict: V3 block data too small" );
    return false;
  }

  const char * ptr = blockData.constData();
  uint8_t compressionEncryption = (uint8_t)ptr[0];
  uint8_t encryptedDataLength = (uint8_t)ptr[1];
  // uint16_t reserved = qFromBigEndian<uint16_t>( (const uchar *)ptr + 2 );
  uint32_t dataCrc = qFromBigEndian<uint32_t>( (const uchar *)ptr + 4 );

  const int headerLength = 8;
  QByteArray rawData = blockData.mid( headerLength );

  V3EncryptionMethod encMethod = static_cast<V3EncryptionMethod>( ( compressionEncryption & 0xF0 ) >> 4 );
  V3CompressionMethod compMethod = static_cast<V3CompressionMethod>( compressionEncryption & 0x0F );

  // Decrypt if needed
  if ( encMethod != V3EncNone && encryptedDataLength > 0 ) {
    QByteArray key = cryptoKey;
    if ( key.isEmpty() ) {
      // Use CRC as key
      QByteArray crcBytes( 4, 0 );
      qToBigEndian( dataCrc, (uchar *)crcBytes.data() );
      key = ripemdDigest( crcBytes );
    }

    if ( encMethod == V3EncSalsa20 ) {
      QByteArray decrypted( encryptedDataLength, 0 );
      salsa20Decrypt( (const uint8_t *)rawData.constData(), 
                      (uint8_t *)decrypted.data(), 
                      encryptedDataLength, 
                      (const uint8_t *)key.constData() );
      rawData.replace( 0, encryptedDataLength, decrypted );
    }
    else if ( encMethod == V3EncSimple ) {
      simpleDecryptV3( (uint8_t *)rawData.data(), encryptedDataLength,
                       (const uint8_t *)key.constData(), key.size() );
    }
  }

  // Decompress
  switch ( compMethod ) {
    case V3CompNone:
      decompressedBlock = rawData;
      break;

    case V3CompLzo: {
      decompressedBlock.resize( decompressedSize );
      lzo_uint outLen = decompressedSize;
      int result = lzo1x_decompress_safe( (const uchar *)rawData.constData(), rawData.size(),
                                          (uchar *)decompressedBlock.data(), &outLen, nullptr );
      if ( result != LZO_E_OK || outLen != decompressedSize ) {
        qWarning( "MDict: V3 LZO decompression failed" );
        return false;
      }
    } break;

    case V3CompDeflate:
      decompressedBlock = zlibDecompress( rawData.constData(), rawData.size(), 0 );
      if ( decompressedBlock.isEmpty() ) {
        qWarning( "MDict: V3 Deflate decompression failed" );
        return false;
      }
      break;

    case V3CompLzma: {
      std::string decompressed = decompressLzma2( rawData.constData(), rawData.size(), false );
      decompressedBlock = QByteArray( decompressed.c_str(), decompressed.size() );
    } break;

    case V3CompBzip2: {
      std::string decompressed = decompressBzip2( rawData.constData(), rawData.size() );
      decompressedBlock = QByteArray( decompressed.c_str(), decompressed.size() );
    } break;

    case V3CompLz4:
      qWarning( "MDict: V3 LZ4 compression not yet supported" );
      return false;

    default:
      qWarning( "MDict: V3 unknown compression method %d", compMethod );
      return false;
  }

  return true;
}

QByteArray MdictParser::readStorageBlockV3( QDataStream & in )
{
  quint32 originalDataLength;
  quint32 dataBlockLength;
  in >> originalDataLength;
  in >> dataBlockLength;

  QByteArray rawData( dataBlockLength, 0 );
  in.readRawData( rawData.data(), dataBlockLength );

  QByteArray decompressed;
  if ( !parseCompressedBlockV3( rawData, cryptoKey_, originalDataLength, decompressed ) ) {
    return QByteArray();
  }

  return decompressed;
}

bool MdictParser::openV3()
{
  QDataStream in( file_ );
  in.setByteOrder( QDataStream::BigEndian );

  // V3 unit order: Content -> ContentBlockIndex -> Key -> KeyBlockIndex
  if ( !readContentUnitV3( in ) ) {
    qWarning( "MDict: Failed to read content unit" );
    return false;
  }

  if ( !readContentBlockIndexUnitV3( in ) ) {
    qWarning( "MDict: Failed to read content block index unit" );
    return false;
  }

  if ( !readKeyUnitV3( in ) ) {
    qWarning( "MDict: Failed to read key unit" );
    return false;
  }

  if ( !readKeyBlockIndexUnitV3( in ) ) {
    qWarning( "MDict: Failed to read key block index unit" );
    return false;
  }

  // Initialize iteration state
  currentKeyBlockIndex_ = 0;
  currentEntryNo_ = 0;

  return true;
}

bool MdictParser::readContentUnitV3( QDataStream & in )
{
  // Read unit info section (20 bytes)
  quint8 unitType;
  in >> unitType;
  
  // Skip reserved bytes (3 + 8 = 11 bytes)
  in.skipRawData( 11 );
  
  in >> contentBlockCount_;
  in >> contentDataSize_;

  contentDataOffset_ = file_->pos();

  // Skip to end of content data section
  file_->seek( contentDataOffset_ + contentDataSize_ );

  // Read data info block (contains record count in XML)
  QByteArray dataInfoBlock = readStorageBlockV3( in );
  if ( dataInfoBlock.isEmpty() ) {
    return false;
  }

  // Parse XML to get record count
  QString dataInfoXml = QString::fromUtf8( dataInfoBlock );
  QDomDocument doc;
  if ( doc.setContent( dataInfoXml ) ) {
    QDomElement root = doc.documentElement();
    wordCount_ = root.attribute( "recordCount", "0" ).toUInt();
  }

  return true;
}

bool MdictParser::readContentBlockIndexUnitV3( QDataStream & in )
{
  // Read unit info section
  quint8 unitType;
  in >> unitType;
  in.skipRawData( 11 );  // reserved
  
  quint32 blockCount;
  quint64 dataSectionLength;
  in >> blockCount;
  in >> dataSectionLength;

  qint64 curPos = file_->pos();
  file_->seek( curPos + dataSectionLength );

  // Read data info block
  QByteArray dataInfoBlock = readStorageBlockV3( in );
  qint64 endPos = file_->pos();

  // Go back and read block index data
  file_->seek( curPos );
  QByteArray indexBlock = readStorageBlockV3( in );
  if ( indexBlock.isEmpty() ) {
    return false;
  }

  // Parse block indexes
  QDataStream indexStream( indexBlock );
  indexStream.setByteOrder( QDataStream::BigEndian );

  quint64 blockOffsetInUnit = 0;
  quint64 blockOffsetInDecompressed = 0;

  contentBlockIndexesV3_.clear();
  contentBlockIndexesV3_.reserve( contentBlockCount_ );

  for ( quint32 i = 0; i < contentBlockCount_; ++i ) {
    V3::ContentBlockIndex blockIndex;
    indexStream >> blockIndex.blockCompressedSize;
    indexStream >> blockIndex.blockDecompressedSize;
    blockIndex.blockOffsetInContentData = blockOffsetInUnit;
    blockIndex.blockOffsetInDecompressed = blockOffsetInDecompressed;

    blockOffsetInUnit += blockIndex.blockCompressedSize;
    blockOffsetInDecompressed += blockIndex.blockDecompressedSize;

    contentBlockIndexesV3_.push_back( blockIndex );
  }

  totalContentDecompressedSize_ = blockOffsetInDecompressed;
  file_->seek( endPos );

  return true;
}

bool MdictParser::readKeyUnitV3( QDataStream & in )
{
  // Read unit info section
  quint8 unitType;
  in >> unitType;
  in.skipRawData( 11 );  // reserved

  quint32 blockCount;
  quint64 dataSectionLength;
  in >> blockCount;
  in >> dataSectionLength;

  keyDataOffset_ = file_->pos();
  keyDataSize_ = dataSectionLength;

  // Skip to end of key data section
  file_->seek( keyDataOffset_ + keyDataSize_ );

  // Read data info block
  QByteArray dataInfoBlock = readStorageBlockV3( in );

  return true;
}

bool MdictParser::readKeyBlockIndexUnitV3( QDataStream & in )
{
  // Read unit info section
  quint8 unitType;
  in >> unitType;
  in.skipRawData( 11 );  // reserved

  quint32 blockCount;
  quint64 dataSectionLength;
  in >> blockCount;
  in >> dataSectionLength;

  qint64 curPos = file_->pos();
  file_->seek( curPos + dataSectionLength );

  // Read data info block
  QByteArray dataInfoBlock = readStorageBlockV3( in );
  qint64 endPos = file_->pos();

  // Go back and read block index data
  file_->seek( curPos );
  QByteArray indexBlock = readStorageBlockV3( in );
  if ( indexBlock.isEmpty() ) {
    return false;
  }

  // Parse block indexes
  QDataStream indexStream( indexBlock );
  indexStream.setByteOrder( QDataStream::BigEndian );

  quint64 blockOffset = 0;
  qint64 firstEntryNo = 0;

  keyBlockIndexesV3_.clear();
  keyBlockIndexesV3_.reserve( blockCount );

  for ( quint32 i = 0; i < blockCount; ++i ) {
    V3::KeyBlockIndex blockIndex;

    // Read entry count
    indexStream >> blockIndex.entryCount;

    // Read first key
    quint16 firstKeyLen;
    indexStream >> firstKeyLen;
    if ( isUtf16Encoding_ ) firstKeyLen *= 2;
    QByteArray firstKeyData( firstKeyLen + (isUtf16Encoding_ ? 2 : 1), 0 );
    indexStream.readRawData( firstKeyData.data(), firstKeyData.size() );
    firstKeyData.truncate( firstKeyLen );
    blockIndex.firstKey = isUtf16Encoding_ 
      ? QString::fromUtf16( (const char16_t *)firstKeyData.constData(), firstKeyLen / 2 )
      : QString::fromUtf8( firstKeyData );

    // Read last key
    quint16 lastKeyLen;
    indexStream >> lastKeyLen;
    if ( isUtf16Encoding_ ) lastKeyLen *= 2;
    QByteArray lastKeyData( lastKeyLen + (isUtf16Encoding_ ? 2 : 1), 0 );
    indexStream.readRawData( lastKeyData.data(), lastKeyData.size() );
    lastKeyData.truncate( lastKeyLen );
    blockIndex.lastKey = isUtf16Encoding_
      ? QString::fromUtf16( (const char16_t *)lastKeyData.constData(), lastKeyLen / 2 )
      : QString::fromUtf8( lastKeyData );

    // Read block sizes
    indexStream >> blockIndex.blockCompressedSize;
    indexStream >> blockIndex.blockDecompressedSize;

    blockIndex.blockOffsetInKeyData = blockOffset;
    blockIndex.firstEntryNo = firstEntryNo;

    blockOffset += blockIndex.blockCompressedSize;
    firstEntryNo += blockIndex.entryCount;

    keyBlockIndexesV3_.push_back( blockIndex );
  }

  file_->seek( endPos );
  return true;
}

bool MdictParser::readNextHeadWordIndexV3( HeadWordIndex & headWordIndex )
{
  headWordIndex.clear();

  if ( currentKeyBlockIndex_ >= keyBlockIndexesV3_.size() ) {
    return false;
  }

  const V3::KeyBlockIndex & blockIndex = keyBlockIndexesV3_[ currentKeyBlockIndex_ ];

  // Seek to key block and read it
  file_->seek( keyDataOffset_ + blockIndex.blockOffsetInKeyData );
  QDataStream in( file_ );
  in.setByteOrder( QDataStream::BigEndian );

  // Read storage block
  QByteArray blockData = readStorageBlockV3( in );
  if ( blockData.isEmpty() ) {
    return false;
  }

  // Parse key entries from block data
  const char * data = blockData.constData();
  int dataLen = blockData.size();
  int offset = 0;

  for ( quint64 i = 0; i < blockIndex.entryCount && offset < dataLen; ++i ) {
    // Read content offset (8 bytes in v3)
    if ( offset + 8 > dataLen ) break;
    qint64 contentOffset = qFromBigEndian<qint64>( (const uchar *)data + offset );
    offset += 8;

    // Read null-terminated key string
    QString key;
    if ( isUtf16Encoding_ ) {
      int keyStart = offset;
      while ( offset + 1 < dataLen ) {
        if ( data[offset] == 0 && data[offset + 1] == 0 ) {
          break;
        }
        offset += 2;
      }
      key = QString::fromUtf16( (const char16_t *)(data + keyStart), (offset - keyStart) / 2 );
      offset += 2;  // Skip null terminator
    }
    else {
      int keyStart = offset;
      while ( offset < dataLen && data[offset] != 0 ) {
        offset++;
      }
      key = QString::fromUtf8( data + keyStart, offset - keyStart );
      offset++;  // Skip null terminator
    }

    headWordIndex.emplace_back( contentOffset, key );
  }

  currentKeyBlockIndex_++;
  return true;
}

bool MdictParser::readRecordBlockV3( HeadWordIndex & headWordIndex, RecordHandler & recordHandler )
{
  for ( size_t i = 0; i < headWordIndex.size(); ++i ) {
    qint64 contentOffset = headWordIndex[i].first;
    const QString & headWord = headWordIndex[i].second;

    // Find content block for this offset
    size_t blockIdx = 0;
    for ( size_t j = 0; j < contentBlockIndexesV3_.size(); ++j ) {
      const V3::ContentBlockIndex & idx = contentBlockIndexesV3_[j];
      if ( contentOffset >= (qint64)idx.blockOffsetInDecompressed &&
           contentOffset < (qint64)(idx.blockOffsetInDecompressed + idx.blockDecompressedSize) ) {
        blockIdx = j;
        break;
      }
    }

    const V3::ContentBlockIndex & blockIndex = contentBlockIndexesV3_[ blockIdx ];

    // Calculate record size
    qint64 recordSize;
    if ( i + 1 < headWordIndex.size() ) {
      recordSize = headWordIndex[i + 1].first - contentOffset;
    }
    else {
      // Last entry in this block
      recordSize = (blockIndex.blockOffsetInDecompressed + blockIndex.blockDecompressedSize) - contentOffset;
    }

    RecordInfo recordInfo;
    recordInfo.compressedBlockPos = contentDataOffset_ + blockIndex.blockOffsetInContentData;
    recordInfo.recordOffset = contentOffset - blockIndex.blockOffsetInDecompressed;
    recordInfo.decompressedBlockSize = blockIndex.blockDecompressedSize;
    recordInfo.compressedBlockSize = blockIndex.blockCompressedSize;
    recordInfo.recordSize = recordSize;

    recordHandler.handleRecord( headWord, recordInfo );
  }

  return true;
}

} // namespace Mdict
