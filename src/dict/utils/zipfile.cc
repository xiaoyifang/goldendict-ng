/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "zipfile.hh"
#include <QtEndian>
#include <QByteArray>
#include <QFileInfo>

namespace ZipFile {

#pragma pack( push, 1 )

/// End-of-central-directory record, as is
struct EndOfCdirRecord
{
  quint32 signature;
  quint16 numDisk, numDiskCd, totalEntriesDisk, totalEntries;
  quint32 size, offset;
  quint16 commentLength;
}
#ifndef _MSC_VER
__attribute__( ( packed ) )
#endif
;

struct CentralFileHeaderRecord
{
  quint32 signature;
  quint16 verMadeBy, verNeeded, gpBits, compressionMethod, fileTime, fileDate;
  quint32 crc32, compressedSize, uncompressedSize;
  quint16 fileNameLength, extraFieldLength, fileCommentLength, diskNumberStart, intFileAttrs;
  quint32 externalFileAttrs, offsetOfLocalHeader;
}
#ifndef _MSC_VER
__attribute__( ( packed ) )
#endif
;

struct LocalFileHeaderRecord
{
  quint32 signature;
  quint16 verNeeded, gpBits, compressionMethod, fileTime, fileDate;
  quint32 crc32, compressedSize, uncompressedSize;
  quint16 fileNameLength, extraFieldLength;
}
#ifndef _MSC_VER
__attribute__( ( packed ) )
#endif
;

#pragma pack( pop )

static quint32 const endOfCdirRecordSignatureValue = qToLittleEndian( 0x06054b50 );
static const quint32 centralFileHeaderSignature    = qToLittleEndian( 0x02014b50 );
static const quint32 localFileHeaderSignature      = qToLittleEndian( 0x04034b50 );

static CompressionMethod getCompressionMethod( quint16 compressionMethod )
{
  switch ( qFromLittleEndian( compressionMethod ) ) {
    case 0:
      return Uncompressed;
    case 8:
      return Deflated;
    default:
      return Unsupported;
  }
}

namespace {

/// Values possibly stored in the Zip64 extended information extra field
/// (header id 0x0001, APPNOTE 4.5.3).
struct Zip64ExtraValues
{
  quint64 uncompressedSize  = 0;
  quint64 compressedSize    = 0;
  quint64 localHeaderOffset = 0;
  quint32 diskNumberStart   = 0;
};

/// Parses the Zip64 extended information extra field out of an extra field
/// buffer. In Zip64 archives the fields of the central directory / local
/// records hold marker values (0xFFFFFFFF for sizes and offsets, 0xFFFF for
/// the disk number) and the real values are moved into this extra field,
/// in exactly the order of the "need" flags below.
Zip64ExtraValues parseZip64Extra( const QByteArray & extra,
                                  bool needUncompressedSize,
                                  bool needCompressedSize,
                                  bool needLocalHeaderOffset,
                                  bool needDiskNumber )
{
  Zip64ExtraValues v;

  int pos = 0;
  while ( pos + 4 <= extra.size() ) {
    quint16 headerId, dataSize;
    memcpy( &headerId, extra.constData() + pos, sizeof( headerId ) );
    memcpy( &dataSize, extra.constData() + pos + 2, sizeof( dataSize ) );
    pos += 4;

    if ( qFromLittleEndian( headerId ) == 0x0001 ) {
      const char * p = extra.constData() + pos;
      int remaining  = qMin( (int)qFromLittleEndian( dataSize ), (int)( extra.size() - pos ) );

      if ( needUncompressedSize && remaining >= 8 ) {
        memcpy( &v.uncompressedSize, p, sizeof( quint64 ) );
        v.uncompressedSize = qFromLittleEndian( v.uncompressedSize );
        p += 8;
        remaining -= 8;
      }
      if ( needCompressedSize && remaining >= 8 ) {
        memcpy( &v.compressedSize, p, sizeof( quint64 ) );
        v.compressedSize = qFromLittleEndian( v.compressedSize );
        p += 8;
        remaining -= 8;
      }
      if ( needLocalHeaderOffset && remaining >= 8 ) {
        memcpy( &v.localHeaderOffset, p, sizeof( quint64 ) );
        v.localHeaderOffset = qFromLittleEndian( v.localHeaderOffset );
        p += 8;
        remaining -= 8;
      }
      if ( needDiskNumber && remaining >= 4 ) {
        memcpy( &v.diskNumberStart, p, sizeof( quint32 ) );
        v.diskNumberStart = qFromLittleEndian( v.diskNumberStart );
      }
      break;
    }
    pos += qFromLittleEndian( dataSize );
  }
  return v;
}

/// Safely narrows a 64-bit zip value to the 32-bit value the index can hold.
/// Values beyond 4GB can't be addressed by the 32-bit offsets and will fail
/// on load; warn once so the problematic archive is identifiable.
quint32 toUint32( quint64 value, const char * what )
{
  static bool warned = false;
  if ( value > 0xFFFFFFFFULL && !warned ) {
    qWarning( "Zip warning: %s exceeds 4GB, such archive entries won't load", what );
    warned = true;
  }
  return (quint32)value;
}

} // anonymous namespace

bool positionAtCentralDir( SplitZipFile & zip )
{
  // Find the end-of-central-directory record

  int maxEofBufferSize = 65535 + sizeof( EndOfCdirRecord );

  if ( zip.size() > maxEofBufferSize ) {
    zip.seek( zip.size() - maxEofBufferSize );
  }
  else if ( (size_t)zip.size() < sizeof( EndOfCdirRecord ) ) {
    return false;
  }
  else {
    zip.seek( 0 );
  }

  QByteArray eocBuffer = zip.read( maxEofBufferSize );

  if ( eocBuffer.size() < (int)sizeof( EndOfCdirRecord ) ) {
    return false;
  }

  int lastIndex = eocBuffer.size() - sizeof( EndOfCdirRecord );

  QByteArray endOfCdirRecordSignature( (const char *)&endOfCdirRecordSignatureValue,
                                       sizeof( endOfCdirRecordSignatureValue ) );

  EndOfCdirRecord endOfCdirRecord;

  quint32 cdir_offset;

  for ( ;; --lastIndex ) {
    lastIndex = eocBuffer.lastIndexOf( endOfCdirRecordSignature, lastIndex );

    if ( lastIndex == -1 ) {
      return false;
    }

    /// We need to copy it due to possible alignment issues on ARM etc
    memcpy( &endOfCdirRecord, eocBuffer.data() + lastIndex, sizeof( endOfCdirRecord ) );

    /// Sanitize the record by checking the offset

    cdir_offset = zip.calcAbsoluteOffset( qFromLittleEndian( endOfCdirRecord.offset ),
                                          qFromLittleEndian( endOfCdirRecord.numDiskCd ) );

    if ( !zip.seek( cdir_offset ) ) {
      continue;
    }

    quint32 signature;

    if ( zip.read( (char *)&signature, sizeof( signature ) ) != sizeof( signature ) ) {
      continue;
    }

    if ( signature == centralFileHeaderSignature ) {
      break;
    }
  }

  // Found cdir -- position the file on the first header

  return zip.seek( cdir_offset );
}

bool readNextEntry( SplitZipFile & zip, CentralDirEntry & entry )
{
  CentralFileHeaderRecord record;

  auto centralDirOffset = zip.pos();

  if ( zip.read( (char *)&record, sizeof( record ) ) != sizeof( record ) ) {
    return false;
  }

  if ( record.signature != centralFileHeaderSignature ) {
    return false;
  }

  // Read file name

  int fileNameLength = qFromLittleEndian( record.fileNameLength );
  entry.fileName     = zip.read( fileNameLength );

  if ( entry.fileName.size() != fileNameLength ) {
    return false;
  }

  // Read the extra field: in Zip64 archives it holds the real values of the
  // fields which contain the marker values. Then skip the file comment.

  const qint64 extraFieldPos  = zip.pos();
  const int extraFieldLength  = qFromLittleEndian( record.extraFieldLength );
  const QByteArray extraField = zip.read( extraFieldLength );

  if ( !zip.seek( extraFieldPos + extraFieldLength + qFromLittleEndian( record.fileCommentLength ) ) ) {
    return false;
  }

  quint16 diskNumberStart   = qFromLittleEndian( record.diskNumberStart );
  quint64 localHeaderOffset = qFromLittleEndian( record.offsetOfLocalHeader );
  quint64 uncompressedSize  = qFromLittleEndian( record.uncompressedSize );
  quint64 compressedSize    = qFromLittleEndian( record.compressedSize );

  if ( diskNumberStart == 0xFFFF || localHeaderOffset == 0xFFFFFFFF || uncompressedSize == 0xFFFFFFFF
       || compressedSize == 0xFFFFFFFF ) {
    // At least one field holds the Zip64 marker -- the real values are in
    // the Zip64 extended information extra field.
    const Zip64ExtraValues zip64 = parseZip64Extra( extraField,
                                                    uncompressedSize == 0xFFFFFFFF,
                                                    compressedSize == 0xFFFFFFFF,
                                                    localHeaderOffset == 0xFFFFFFFF,
                                                    diskNumberStart == 0xFFFF );
    if ( uncompressedSize == 0xFFFFFFFF ) {
      uncompressedSize = zip64.uncompressedSize;
    }
    if ( compressedSize == 0xFFFFFFFF ) {
      compressedSize = zip64.compressedSize;
    }
    if ( localHeaderOffset == 0xFFFFFFFF ) {
      localHeaderOffset = zip64.localHeaderOffset;
    }
    if ( diskNumberStart == 0xFFFF ) {
      diskNumberStart = zip64.diskNumberStart;
    }
  }

  // The position of the central directory record is the absolute position it
  // was read from -- no disk number arithmetic applies to it.
  entry.centralHeaderOffset = toUint32( centralDirOffset, "central directory offset" );

  entry.localHeaderOffset =
    toUint32( zip.calcAbsoluteOffset( localHeaderOffset, diskNumberStart ), "local header offset" );
  entry.compressedSize    = toUint32( compressedSize, "compressed size" );
  entry.uncompressedSize  = toUint32( uncompressedSize, "uncompressed size" );
  entry.compressionMethod = getCompressionMethod( record.compressionMethod );
  entry.fileNameInUTF8    = ( qFromLittleEndian( record.gpBits ) & 0x800 ) != 0;

  return true;
}

bool skipLocalHeader( SplitZipFile & zip )
{
  LocalFileHeaderRecord record;

  if ( zip.read( (char *)&record, sizeof( record ) ) != sizeof( record ) ) {
    return false;
  }

  if ( record.signature != localFileHeaderSignature ) {
    return false;
  }

  // skip file name
  int fileNameLength = qFromLittleEndian( record.fileNameLength );
  // Skip extra field
  return zip.seek( zip.pos() + fileNameLength + qFromLittleEndian( record.extraFieldLength ) );
}

bool readLocalHeaderFromCentral( SplitZipFile & zip, LocalFileHeader & entry )
{
  CentralFileHeaderRecord record;

  if ( zip.read( (char *)&record, sizeof( record ) ) != sizeof( record ) ) {
    return false;
  }

  if ( record.signature != centralFileHeaderSignature ) {
    return false;
  }

  // Read file name

  int fileNameLength = qFromLittleEndian( record.fileNameLength );
  entry.fileName     = zip.read( fileNameLength );

  if ( entry.fileName.size() != fileNameLength ) {
    return false;
  }

  // Read the extra field: in Zip64 archives it holds the real values of the
  // fields which contain the marker values.

  const int extraFieldLength  = qFromLittleEndian( record.extraFieldLength );
  const QByteArray extraField = zip.read( extraFieldLength );

  quint16 diskNumberStart   = qFromLittleEndian( record.diskNumberStart );
  quint64 localHeaderOffset = qFromLittleEndian( record.offsetOfLocalHeader );
  quint64 uncompressedSize  = qFromLittleEndian( record.uncompressedSize );
  quint64 compressedSize    = qFromLittleEndian( record.compressedSize );

  if ( diskNumberStart == 0xFFFF || localHeaderOffset == 0xFFFFFFFF || uncompressedSize == 0xFFFFFFFF
       || compressedSize == 0xFFFFFFFF ) {
    const Zip64ExtraValues zip64 = parseZip64Extra( extraField,
                                                    uncompressedSize == 0xFFFFFFFF,
                                                    compressedSize == 0xFFFFFFFF,
                                                    localHeaderOffset == 0xFFFFFFFF,
                                                    diskNumberStart == 0xFFFF );
    if ( uncompressedSize == 0xFFFFFFFF ) {
      uncompressedSize = zip64.uncompressedSize;
    }
    if ( compressedSize == 0xFFFFFFFF ) {
      compressedSize = zip64.compressedSize;
    }
    if ( localHeaderOffset == 0xFFFFFFFF ) {
      localHeaderOffset = zip64.localHeaderOffset;
    }
    if ( diskNumberStart == 0xFFFF ) {
      diskNumberStart = zip64.diskNumberStart;
    }
  }

  entry.compressedSize    = toUint32( compressedSize, "compressed size" );
  entry.uncompressedSize  = toUint32( uncompressedSize, "uncompressed size" );
  entry.compressionMethod = getCompressionMethod( record.compressionMethod );
  entry.offset = toUint32( zip.calcAbsoluteOffset( localHeaderOffset, diskNumberStart ), "local header offset" );

  return true;
}

SplitZipFile::SplitZipFile( const QString & name )
{
  setFileName( name );
}

void SplitZipFile::setFileName( const QString & name )
{
  {
    QString lname = name.toLower();
    if ( lname.endsWith( ".zips" ) ) {
      appendFile( name );
      return;
    }

    if ( !lname.endsWith( ".zip" ) ) {
      return;
    }
  }

  if ( QFileInfo( name ).isFile() ) {
    for ( int i = 1; i < 100; i++ ) {
      QString name2 = name.left( name.size() - 2 ) + QString( "%1" ).arg( i, 2, 10, QChar( '0' ) );
      if ( QFileInfo( name2 ).isFile() ) {
        appendFile( name2 );
      }
      else {
        break;
      }
    }
    appendFile( name );
  }
  else {
    for ( int i = 1; i < 1000; i++ ) {
      QString name2 = name + QString( ".%1" ).arg( i, 3, 10, QChar( '0' ) );
      if ( QFileInfo( name2 ).isFile() ) {
        appendFile( name2 );
      }
      else {
        break;
      }
    }
  }
}

QDateTime SplitZipFile::lastModified() const
{
  unsigned long ts = 0;
  for ( QList< QFile * >::const_iterator i = files.begin(); i != files.end(); ++i ) {
    unsigned long t = QFileInfo( ( *i )->fileName() ).lastModified().toSecsSinceEpoch();
    if ( t > ts ) {
      ts = t;
    }
  }
  return QDateTime::fromSecsSinceEpoch( ts );
}

qint64 SplitZipFile::calcAbsoluteOffset( qint64 offset, quint16 partNo )
{
  if ( partNo >= offsets.size() ) {
    // Bogus disk number, or the Zip64 0xFFFF marker ("the real value is in
    // the extra field") on a single volume archive. Fall back to the first
    // part instead of silently producing a zero offset.
    partNo = 0;
  }

  return offsets.at( partNo ) + offset;
}

} // namespace ZipFile
