/* This file is (c) 2015 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */


#include "slob.hh"
#include "btreeidx.hh"

#include "folding.hh"
#include "text.hh"
#include "decompress.hh"
#include "langcoder.hh"
#include "ftshelpers.hh"
#include "htmlescape.hh"
#include "filetype.hh"
#include "tiff.hh"
#include "utils.hh"
#include "iconv.hh"
#include <QString>
#include <QStringBuilder>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QProcess>
#include <QList>
#include <QtEndian>
#include <QRegularExpression>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <set>
#include <algorithm>

namespace Slob {

using std::string;
using std::map;
using std::vector;
using std::multimap;
using std::pair;
using std::set;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

DEF_EX_STR( exNotSlobFile, "Not an Slob file", Dictionary::Ex )
DEF_EX( exTruncateFile, "Slob file truncated", Dictionary::Ex )
using Dictionary::exCantReadFile;
DEF_EX_STR( exCantDecodeFile, "Can't decode file", Dictionary::Ex )
DEF_EX_STR( exNoCodecFound, "No text codec found", Dictionary::Ex )
DEF_EX( exUserAbort, "User abort", Dictionary::Ex )
DEF_EX( exNoResource, "No resource found", Dictionary::Ex )


enum {
  Signature            = 0x58424C53, // SLBX on little-endian, XBLS on big-endian
  CurrentFormatVersion = 2 + BtreeIndexing::FormatVersion + Folding::Version
};

#pragma pack( push, 1 )
struct IdxHeader
{
  quint32 signature;             // First comes the signature, SLBX
  quint32 formatVersion;         // File format version (CurrentFormatVersion)
  quint32 indexBtreeMaxElements; // Two fields from IndexInfo
  quint32 indexRootOffset;
  quint32 resourceIndexBtreeMaxElements; // Two fields from IndexInfo
  quint32 resourceIndexRootOffset;
  quint32 wordCount;
  quint32 articleCount;
  quint32 langFrom; // Source language
  quint32 langTo;   // Target language
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

const char SLOB_MAGIC[ 8 ] = { 0x21, 0x2d, 0x31, 0x53, 0x4c, 0x4f, 0x42, 0x1f };

struct RefEntry
{
  QString key;
  quint32 itemIndex;
  quint16 binIndex;
  QString fragment;
};

bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}


class SlobFile
{
public:
  using RefEntryOffsetItem = std::pair< quint64, quint32 >;
  using RefOffsetsVector   = QList< RefEntryOffsetItem >;

private:
  enum Compressions {
    UNKNOWN = 0,
    NONE,
    ZLIB,
    BZ2,
    LZMA2
  };

  QFile file;
  QString fileName, dictionaryName;
  Compressions compression;
  std::string encoding;
  unsigned char uuid[ 16 ];
  QMap< QString, QString > tags;
  QList< QString > contentTypes;
  quint32 blobCount;
  quint64 storeOffset, fileSize, refsOffset;
  quint32 refsCount, itemsCount;
  quint64 itemsOffset, itemsDataOffset;
  quint32 currentItem;
  quint32 contentTypesCount;
  string currentItemData;
  RefOffsetsVector refsOffsetVector;

  QString readTinyText();
  QString readText();
  QString readLargeText();
  QString readString( unsigned length );

public:
  SlobFile():
    compression( UNKNOWN ),
    blobCount( 0 ),
    storeOffset( 0 ),
    fileSize( 0 ),
    refsOffset( 0 ),
    refsCount( 0 ),
    itemsCount( 0 ),
    itemsOffset( 0 ),
    itemsDataOffset( 0 ),
    currentItem( 0xFFFFFFFF ),
    contentTypesCount( 0 )
  {
  }

  ~SlobFile();

  Compressions getCompression() const
  {
    return compression;
  }

  std::string const & getEncoding() const
  {
    return encoding;
  }

  QString const & getDictionaryName() const
  {
    return dictionaryName;
  }

  quint32 blobsCount() const
  {
    return blobCount;
  }

  quint64 dataOffset() const
  {
    return storeOffset;
  }

  quint32 getRefsCount() const
  {
    return refsCount;
  }

  quint32 getContentTypesCount() const
  {
    return contentTypesCount;
  }

  const RefOffsetsVector & getSortedRefOffsets();

  void clearRefOffsets()
  {
    refsOffsetVector.clear();
  }

  QString getContentType( quint8 content_id ) const
  {
    return content_id < contentTypes.size() ? contentTypes[ content_id ] : QString();
  }

  QMap< QString, QString > const & getTags() const
  {
    return tags;
  }

  void open( const QString & name );

  void getRefEntryAtOffset( quint64 offset, RefEntry & entry );

  void getRefEntry( quint32 ref_nom, RefEntry & entry );

  quint8 getItem( RefEntry const & entry, string * data );
};

SlobFile::~SlobFile()
{
  file.close();
}

QString SlobFile::readString( unsigned length )
{
  QByteArray data = file.read( length );
  QString str;

  if ( !encoding.empty() && !data.isEmpty() ) {
    try {
      str = Iconv::toQString( encoding.c_str(), data.data(), data.size() );
    }
    catch ( Iconv::Ex & e ) {
      qDebug() << QString( R"(slob decoding failed: %1)" ).arg( e.what() );
    }
  }
  else {
    str = QString( data );
  }

  char term = 0;
  int n     = str.indexOf( term );
  if ( n >= 0 ) {
    str.resize( n );
  }

  return str;
}

QString SlobFile::readTinyText()
{
  unsigned char len;
  if ( !file.getChar( (char *)&len ) ) {
    QString error = fileName + ": " + file.errorString();
    throw exCantReadFile( string( error.toUtf8().data() ) );
  }
  return readString( len );
}

QString SlobFile::readText()
{
  quint16 len;
  if ( file.read( (char *)&len, sizeof( len ) ) != sizeof( len ) ) {
    QString error = fileName + ": " + file.errorString();
    throw exCantReadFile( string( error.toUtf8().data() ) );
  }
  return readString( qFromBigEndian( len ) );
}

QString SlobFile::readLargeText()
{
  quint32 len;
  if ( file.read( (char *)&len, sizeof( len ) ) != sizeof( len ) ) {
    QString error = fileName + ": " + file.errorString();
    throw exCantReadFile( string( error.toUtf8().data() ) );
  }
  return readString( qFromBigEndian( len ) );
}

void SlobFile::open( const QString & name )
{
  QString error( name + ": " );

  if ( file.isOpen() ) {
    file.close();
  }

  fileName = name;

  file.setFileName( name );

  {
    QFileInfo fi( name );
    dictionaryName = fi.fileName();
  }

  for ( ;; ) {

    if ( !file.open( QFile::ReadOnly ) ) {
      break;
    }

    char magic[ 8 ];
    if ( file.read( magic, sizeof( magic ) ) != sizeof( magic ) ) {
      break;
    }

    if ( memcmp( magic, SLOB_MAGIC, sizeof( magic ) ) != 0 ) {
      throw exNotSlobFile( string( name.toUtf8().data() ) );
    }

    if ( file.read( (char *)uuid, sizeof( uuid ) ) != sizeof( uuid ) ) {
      break;
    }

    // Read encoding

    encoding = readTinyText().toStdString();

    // Read compression type

    QString compr = readTinyText();

    if ( compr.compare( "zlib", Qt::CaseInsensitive ) == 0 ) {
      compression = ZLIB;
    }
    else if ( compr.compare( "bz2", Qt::CaseInsensitive ) == 0 ) {
      compression = BZ2;
    }
    else if ( compr.compare( "lzma2", Qt::CaseInsensitive ) == 0 ) {
      compression = LZMA2;
    }
    else if ( compr.isEmpty() || compr.compare( "none", Qt::CaseInsensitive ) == 0 ) {
      compression = NONE;
    }

    // Read tags

    unsigned char count;
    if ( !file.getChar( (char *)&count ) ) {
      break;
    }

    for ( unsigned i = 0; i < count; i++ ) {
      QString key   = readTinyText();
      QString value = readTinyText();
      tags[ key ]   = value;

      if ( key.compare( "label", Qt::CaseInsensitive ) == 0 || key.compare( "name", Qt::CaseInsensitive ) == 0 ) {
        dictionaryName = value;
      }
    }

    // Read content types

    if ( !file.getChar( (char *)&count ) ) {
      break;
    }

    for ( unsigned i = 0; i < count; i++ ) {
      QString type = readText();
      contentTypes.append( type );
    }
    contentTypesCount = count;

    // Read data parameters

    quint32 cnt;
    if ( file.read( (char *)&cnt, sizeof( cnt ) ) != sizeof( cnt ) ) {
      break;
    }
    blobCount = qFromBigEndian( cnt );

    quint64 tmp;
    if ( file.read( (char *)&tmp, sizeof( tmp ) ) != sizeof( tmp ) ) {
      break;
    }
    storeOffset = qFromBigEndian( tmp );

    if ( file.read( (char *)&tmp, sizeof( tmp ) ) != sizeof( tmp ) ) {
      break;
    }
    fileSize = qFromBigEndian( tmp );

    //truncated file
    if ( file.size() < fileSize ) {
      throw exTruncateFile();
    }

    if ( file.read( (char *)&cnt, sizeof( cnt ) ) != sizeof( cnt ) ) {
      break;
    }
    refsCount = qFromBigEndian( cnt );

    refsOffset = file.pos();

    if ( !file.seek( storeOffset ) ) {
      break;
    }

    if ( file.read( (char *)&cnt, sizeof( cnt ) ) != sizeof( cnt ) ) {
      break;
    }
    itemsCount = qFromBigEndian( cnt );

    itemsOffset     = storeOffset + sizeof( itemsCount );
    itemsDataOffset = itemsOffset + itemsCount * sizeof( quint64 );

    return;
  }
  error += file.errorString();
  throw exCantReadFile( string( error.toUtf8().data() ) );
}

const SlobFile::RefOffsetsVector & SlobFile::getSortedRefOffsets()
{
  quint64 tmp;
  qint64 size  = refsCount * sizeof( quint64 );
  quint64 base = refsOffset + size;

  refsOffsetVector.clear();
  refsOffsetVector.reserve( refsCount );

  for ( ;; ) {
    QByteArray offsets;
    offsets.resize( size );

    if ( !file.seek( refsOffset ) || file.read( offsets.data(), size ) != size ) {
      break;
    }

    for ( quint32 i = 0; i < refsCount; i++ ) {
      memcpy( &tmp, offsets.data() + i * sizeof( quint64 ), sizeof( tmp ) );
      refsOffsetVector.append( RefEntryOffsetItem( base + qFromBigEndian( tmp ), i ) );
    }

    std::sort( refsOffsetVector.begin(), refsOffsetVector.end() );
    return refsOffsetVector;
  }
  QString error = fileName + ": " + file.errorString();
  throw exCantReadFile( string( error.toUtf8().data() ) );
}

void SlobFile::getRefEntryAtOffset( quint64 offset, RefEntry & entry )
{
  for ( ;; ) {
    if ( !file.seek( offset ) ) {
      break;
    }

    entry.key = readText();

    quint32 index;
    if ( file.read( (char *)&index, sizeof( index ) ) != sizeof( index ) ) {
      break;
    }
    entry.itemIndex = qFromBigEndian( index );

    quint16 binIndex;
    if ( file.read( (char *)&binIndex, sizeof( binIndex ) ) != sizeof( binIndex ) ) {
      break;
    }
    entry.binIndex = qFromBigEndian( binIndex );

    entry.fragment = readTinyText();

    return;
  }
  QString error = fileName + ": " + file.errorString();
  throw exCantReadFile( string( error.toUtf8().data() ) );
}

void SlobFile::getRefEntry( quint32 ref_nom, RefEntry & entry )
{
  quint64 pos = refsOffset + ref_nom * sizeof( quint64 );
  quint64 offset, tmp;

  for ( ;; ) {
    if ( !file.seek( pos ) || file.read( (char *)&tmp, sizeof( tmp ) ) != sizeof( tmp ) ) {
      break;
    }

    offset = qFromBigEndian( tmp ) + refsOffset + refsCount * sizeof( quint64 );

    getRefEntryAtOffset( offset, entry );

    return;
  }
  QString error = fileName + ": " + file.errorString();
  throw exCantReadFile( string( error.toUtf8().data() ) );
}

quint8 SlobFile::getItem( RefEntry const & entry, string * data )
{
  quint64 pos = itemsOffset + entry.itemIndex * sizeof( quint64 );
  quint64 offset, tmp;

  for ( ;; ) {
    // Read item data types

    if ( !file.seek( pos ) || file.read( (char *)&tmp, sizeof( tmp ) ) != sizeof( tmp ) ) {
      break;
    }

    offset = qFromBigEndian( tmp ) + itemsDataOffset;

    if ( !file.seek( offset ) ) {
      break;
    }

    quint32 bins, bins_be;
    if ( file.read( (char *)&bins_be, sizeof( bins_be ) ) != sizeof( bins_be ) ) {
      break;
    }
    bins = qFromBigEndian( bins_be );

    if ( entry.binIndex >= bins ) {
      return 0xFF;
    }

    QList< quint8 > ids;
    ids.resize( bins );
    if ( file.read( (char *)ids.data(), bins ) != bins ) {
      break;
    }

    quint8 id = ids[ entry.binIndex ];

    if ( id >= (unsigned)contentTypes.size() ) {
      return 0xFF;
    }

    if ( data != 0 ) {
      // Read item data
      if ( currentItem != entry.itemIndex ) {
        currentItemData.clear();
        quint32 length, length_be;
        if ( file.read( (char *)&length_be, sizeof( length_be ) ) != sizeof( length_be ) ) {
          break;
        }
        length = qFromBigEndian( length_be );

        QByteArray compressedData = file.read( length );

        if ( compression == NONE ) {
          currentItemData = string( compressedData.data(), compressedData.length() );
        }
        else if ( compression == ZLIB ) {
          currentItemData = decompressZlib( compressedData.data(), length );
        }
        else if ( compression == BZ2 ) {
          currentItemData = decompressBzip2( compressedData.data(), length );
        }
        else {
          currentItemData = decompressLzma2( compressedData.data(), length, true );
        }

        if ( currentItemData.empty() ) {
          currentItem = 0xFFFFFFFF;
          return 0xFF;
        }
        currentItem = entry.itemIndex;
      }

      // Find bin data inside item

      const char * ptr = currentItemData.c_str();
      quint32 pos      = entry.binIndex * sizeof( quint32 );

      if ( pos >= currentItemData.length() - sizeof( quint32 ) ) {
        return 0xFF;
      }

      quint32 offset, offset_be;
      memcpy( &offset_be, ptr + pos, sizeof( offset_be ) );
      offset = qFromBigEndian( offset_be );

      pos = bins * sizeof( quint32 ) + offset;

      if ( pos >= currentItemData.length() - sizeof( quint32 ) ) {
        return 0xFF;
      }

      quint32 length, len_be;
      memcpy( &len_be, ptr + pos, sizeof( len_be ) );
      length = qFromBigEndian( len_be );

      *data = currentItemData.substr( pos + sizeof( len_be ), length );
    }

    return ids[ entry.binIndex ];
  }
  QString error = fileName + ": " + file.errorString();
  throw exCantReadFile( string( error.toUtf8().data() ) );
}

// SlobDictionary

class SlobDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  QMutex slobMutex, idxResourceMutex;
  File::Index idx;
  BtreeIndex resourceIndex;
  IdxHeader idxHeader;
  SlobFile sf;

  string idxFileName;

public:

  SlobDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~SlobDictionary();

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.articleCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return idxHeader.wordCount;
  }

  inline quint32 getLangFrom() const override
  {
    return idxHeader.langFrom;
  }

  inline quint32 getLangTo() const override
  {
    return idxHeader.langTo;
  }

  sptr< Dictionary::DataRequest > getArticle( std::u32string const &,
                                              vector< std::u32string > const & alts,
                                              std::u32string const &,
                                              bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

  QString const & getDescription() override;

  /// Loads the resource.
  void loadResource( std::string & resourceName, string & data );

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  quint64 getArticlePos( uint32_t articleNumber );

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else {
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "SLOB", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

  uint32_t getFtsIndexVersion() override
  {
    return 2;
  }

protected:

  void loadIcon() noexcept override;

private:

  /// Loads the article.
  void loadArticle( quint32 address, string & articleText );

  quint32 readArticle( quint32 address, string & articleText, RefEntry & entry );

  string convert( string const & in_data, RefEntry const & entry );

  friend class SlobArticleRequest;
  friend class SlobResourceRequest;
};

SlobDictionary::SlobDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idxFileName( indexFile ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() )
{
  // Open data file
  sf.open( dictionaryFiles[ 0 ].c_str() );

  // Initialize the indexes

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  resourceIndex.openIndex( IndexInfo( idxHeader.resourceIndexBtreeMaxElements, idxHeader.resourceIndexRootOffset ),
                           idx,
                           idxResourceMutex );

  // Read dictionary name

  dictionaryName = sf.getDictionaryName().toStdString();
  if ( dictionaryName.empty() ) {
    dictionaryName = QFileInfo( QString::fromStdString( dictionaryFiles[ 0 ] ) ).fileName().toStdString();
  }

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

SlobDictionary::~SlobDictionary() {}

void SlobDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );


  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_slob.png" );
  }

  dictionaryIconLoaded = true;
}

QString const & SlobDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  for ( auto [ key, value ] : sf.getTags().asKeyValueRange() ) {
    dictionaryDescription += "<b>" % key % "</b>" % ": " % value % "<br>";
  }

  return dictionaryDescription;
}

void SlobDictionary::loadArticle( quint32 address, string & articleText )
{
  articleText.clear();

  RefEntry entry;

  readArticle( address, articleText, entry );

  if ( !articleText.empty() ) {
    articleText = convert( articleText, entry );
  }
  else {
    articleText = QObject::tr( "Article decoding error" ).toStdString();
  }

  // See Issue #271: A mechanism to clean-up invalid HTML cards.
  string cleaner = Utils::Html::getHtmlCleaner();

  string prefix( "<div class=\"slobdict\"" );
  if ( isToLanguageRTL() ) {
    prefix += " dir=\"rtl\"";
  }
  prefix += ">";

  articleText = prefix + articleText + cleaner + "</div>";
}

string SlobDictionary::convert( const string & in, RefEntry const & entry )
{
  QString text = QString::fromUtf8( in.c_str() );

  // pattern of img and script
  text.replace( QRegularExpression( "<\\s*(img|script)\\s+([^>]*)src=\"(?!(?:data|https?|ftp):)(|/)([^\"]*)\"" ),
                QString( R"(<\1 \2src="bres://%1/\4")" ).arg( getId().c_str() ) );

  // pattern <link... href="..." ...>
  text.replace( QRegularExpression( R"(<\s*link\s+([^>]*)href="(?!(?:data|https?|ftp):))" ),
                QString( "<link \\1href=\"bres://%1/" ).arg( getId().c_str() ) );

  // pattern <a href="..." ...>, excluding any known protocols such as http://, mailto:, #(comment)
  // these links will be translated into local definitions
  QString anchor;
  QRegularExpression rxLink(
    R"lit(<\s*a\s+([^>]*)href="(?!(?:\w+://|#|mailto:|tel:))(/|)([^"]*)"\s*(title="[^"]*")?[^>]*>)lit" );
  QRegularExpressionMatchIterator it = rxLink.globalMatch( text );
  int pos                            = 0;
  QString newText;
  while ( it.hasNext() ) {
    QRegularExpressionMatch match = it.next();

    newText += text.mid( pos, match.capturedStart() - pos );
    pos = match.capturedEnd();

    QStringList list = match.capturedTexts();
    // Add empty strings for compatibility with regex behaviour
    for ( int i = match.lastCapturedIndex() + 1; i < 5; i++ ) {
      list.append( QString() );
    }

    QString tag = list[ 3 ];
    if ( !list[ 4 ].isEmpty() ) {
      tag = list[ 4 ].split( "\"" )[ 1 ];
    }

    // Find anchor
    int n = list[ 3 ].indexOf( '#' );
    if ( n > 0 ) {
      anchor = QString( "?gdanchor=" ) + list[ 3 ].mid( n + 1 );
      tag.remove( list[ 3 ].mid( n ) );
    }
    else {
      anchor.clear();
    }

    tag.remove( QRegularExpression( ".*/" ) )
      .remove( QRegularExpression( "\\.(s|)htm(l|)$", QRegularExpression::PatternOption::CaseInsensitiveOption ) )
      .replace( "_", "%20" )
      .prepend( "<a href=\"gdlookup://localhost/" )
      .append( anchor + "\" " + list[ 4 ] + ">" );

    newText += tag;
  }
  if ( pos ) {
    newText += text.mid( pos );
    text = newText;
  }
  newText.clear();

#ifdef Q_OS_WIN32
  // Increase equations scale
  text = QString::fromLatin1( "<script type=\"text/x-mathjax-config\">MathJax.Hub.Config({" )
    + " SVG: { scale: 170, linebreaks: { automatic:true } }"
    + ", \"HTML-CSS\": { scale: 210, linebreaks: { automatic:true } }"
    + ", CommonHTML: { scale: 210, linebreaks: { automatic:true } }" + " });</script>" + text;
#endif

  // Fix outstanding elements
  text += "<br style=\"clear:both;\" />";

  return text.toUtf8().data();
}

void SlobDictionary::loadResource( std::string & resourceName, string & data )
{
  vector< WordArticleLink > link;
  RefEntry entry;

  link = resourceIndex.findArticles( Text::toUtf32( resourceName ) );

  if ( link.empty() ) {
    return;
  }

  readArticle( link[ 0 ].articleOffset, data, entry );
}

quint32 SlobDictionary::readArticle( quint32 articleNumber, std::string & result, RefEntry & entry )
{
  string data;
  quint8 contentId;

  {
    QMutexLocker _( &slobMutex );
    if ( entry.key.isEmpty() ) {
      sf.getRefEntry( articleNumber, entry );
    }
    contentId = sf.getItem( entry, &data );
  }

  if ( contentId == 0xFF ) {
    return 0xFFFFFFFF;
  }

  QString contentType = sf.getContentType( contentId );

  if ( contentType.contains( "text/html", Qt::CaseInsensitive )
       || contentType.contains( "text/plain", Qt::CaseInsensitive )
       || contentType.contains( "/css", Qt::CaseInsensitive )
       || contentType.contains( "/javascript", Qt::CaseInsensitive )
       || contentType.contains( "/json", Qt::CaseInsensitive ) ) {
    QString content;
    try {
      content = Iconv::toQString( sf.getEncoding().c_str(), data.data(), data.size() );
    }
    catch ( Iconv::Ex & e ) {
      qDebug() << QString( R"(slob decoding failed: %1)" ).arg( e.what() );
    }

    result = string( content.toUtf8().data() );
  }
  else {
    result = data;
  }

  return contentId;
}

quint64 SlobDictionary::getArticlePos( uint32_t articleNumber )
{
  RefEntry entry;
  {
    QMutexLocker _( &slobMutex );
    sf.getRefEntry( articleNumber, entry );
  }
  return ( ( (quint64)( entry.binIndex ) ) << 32 ) | entry.itemIndex;
}

void SlobDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) ) {
    FTS_index_completed.ref();
  }

  if ( haveFTSIndex() ) {
    return;
  }

  if ( !ensureInitDone().empty() ) {
    return;
  }


  qDebug( "Slob: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    const auto slob_dic = std::make_unique< SlobDictionary >( getId(), idxFileName, getDictionaryFilenames() );
    FtsHelpers::makeFTSIndex( slob_dic.get(), isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "Slob: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void SlobDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    RefEntry entry;
    string articleText;

    quint32 htmlType = 0xFFFFFFFF;

    for ( unsigned i = 0; i < sf.getContentTypesCount(); i++ ) {
      if ( sf.getContentType( i ).startsWith( "text/html", Qt::CaseInsensitive ) ) {
        htmlType = i;
        break;
      }
    }

    quint32 type = readArticle( articleAddress, articleText, entry );
    headword     = entry.key;

    text = QString::fromUtf8( articleText.data(), articleText.size() );

    if ( type == htmlType ) {
      text = Html::unescape( text );
    }
  }
  catch ( std::exception & ex ) {
    qWarning( "Slob: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}


sptr< Dictionary::DataRequest >
SlobDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}


/// SlobDictionary::getArticle()


class SlobArticleRequest: public Dictionary::DataRequest
{

  std::u32string word;
  vector< std::u32string > alts;
  SlobDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  SlobArticleRequest( std::u32string const & word_,
                      vector< std::u32string > const & alts_,
                      SlobDictionary & dict_,
                      bool ignoreDiacritics_ ):
    word( word_ ),
    alts( alts_ ),
    dict( dict_ ),
    ignoreDiacritics( ignoreDiacritics_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~SlobArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void SlobArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

  for ( const auto & alt : alts ) {
    /// Make an additional query for each alt

    vector< WordArticleLink > altChain = dict.findArticles( alt, ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  multimap< std::u32string, pair< string, string > > mainArticles, alternateArticles;

  set< quint64 > articlesIncluded; // Some synonims make it that the articles
                                   // appear several times. We combat this
                                   // by only allowing them to appear once.

  std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics ) {
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
  }

  for ( auto & x : chain ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      finish();
      return;
    }

    quint64 pos = dict.getArticlePos( x.articleOffset ); // Several "articleOffset" values may refer to one article

    if ( articlesIncluded.find( pos ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Now grab that article

    string headword, articleText;

    headword = x.word;
    try {
      dict.loadArticle( x.articleOffset, articleText );
    }
    catch ( ... ) {
    }

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    std::u32string headwordStripped = Folding::applySimpleCaseOnly( headword );
    if ( ignoreDiacritics ) {
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
    }

    multimap< std::u32string, pair< string, string > > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

    articlesIncluded.insert( pos );
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    // No such word
    finish();
    return;
  }

  string result;

  multimap< std::u32string, pair< string, string > >::const_iterator i;

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    result += R"(<div class="slobdict"><h3 class="slobdict_headword">)";
    result += i->second.first;
    result += "</h3></div>";
    result += i->second.second;
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    result += R"(<div class="slobdict"><h3 class="slobdict_headword">)";
    result += i->second.first;
    result += "</h3></div>";
    result += i->second.second;
  }

  appendString( result );

  hasAnyData = true;

  finish();
}

sptr< Dictionary::DataRequest > SlobDictionary::getArticle( std::u32string const & word,
                                                            vector< std::u32string > const & alts,
                                                            std::u32string const &,
                                                            bool ignoreDiacritics )

{
  return std::make_shared< SlobArticleRequest >( word, alts, *this, ignoreDiacritics );
}

//// SlobDictionary::getResource()

class SlobResourceRequest: public Dictionary::DataRequest
{

  SlobDictionary & dict;

  string resourceName;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  SlobResourceRequest( SlobDictionary & dict_, string const & resourceName_ ):
    dict( dict_ ),
    resourceName( resourceName_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~SlobResourceRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void SlobResourceRequest::run()
{
  // Some runnables linger enough that they are cancelled before they start
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  try {
    string resource;
    dict.loadResource( resourceName, resource );
    if ( resource.empty() ) {
      throw exNoResource();
    }

    if ( Filetype::isNameOfCSS( resourceName ) ) {
      QString css = QString::fromUtf8( resource.data(), resource.size() );
      dict.isolateCSS( css, ".slobdict" );
      QByteArray bytes = css.toUtf8();

      QMutexLocker _( &dataMutex );
      data.resize( bytes.size() );
      memcpy( &data.front(), bytes.constData(), bytes.size() );
    }
    else if ( Filetype::isNameOfTiff( resourceName ) ) {
      // Convert it

      QMutexLocker _( &dataMutex );
      GdTiff::tiff2img( data );
    }
    else {
      QMutexLocker _( &dataMutex );
      data.resize( resource.size() );
      memcpy( &data.front(), resource.data(), data.size() );
    }

    QMutexLocker _( &dataMutex );
    hasAnyData = true;
  }
  catch ( std::exception & ex ) {
    qWarning( "SLOB: Failed loading resource \"%s\" from \"%s\", reason: %s",
              resourceName.c_str(),
              dict.getName().c_str(),
              ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

sptr< Dictionary::DataRequest > SlobDictionary::getResource( string const & name )

{
  return std::make_shared< SlobResourceRequest >( *this, name );
}


vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing,
                                                      unsigned maxHeadwordsToExpand )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Skip files with the extensions different to .slob to speed up the
    // scanning

    QString firstName = QDir::fromNativeSeparators( fileName.c_str() );
    if ( !firstName.endsWith( ".slob" ) ) {
      continue;
    }

    // Got the file -- check if we need to rebuid the index

    vector< string > dictFiles( 1, fileName );

    string dictId = Dictionary::makeDictionaryId( dictFiles );

    string indexFile = indicesDir + dictId;

    try {
      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        SlobFile sf;

        qDebug( "Slob: Building the index for dictionary: %s", fileName.c_str() );

        sf.open( firstName );

        initializing.indexingDictionary( sf.getDictionaryName().toUtf8().constData() );

        File::Index idx( indexFile, QIODevice::WriteOnly );
        IdxHeader idxHeader;
        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        RefEntry refEntry;
        quint32 entries = sf.getRefsCount();

        IndexedWords indexedWords, indexedResources;

        set< quint64 > articlesPos;
        quint32 articleCount = 0, wordCount = 0;

        SlobFile::RefOffsetsVector const & offsets = sf.getSortedRefOffsets();

        for ( quint32 i = 0; i < entries; i++ ) {
          sf.getRefEntryAtOffset( offsets[ i ].first, refEntry );

          quint8 type = sf.getItem( refEntry, 0 );

          QString contentType = sf.getContentType( type );

          if ( contentType.startsWith( "text/html", Qt::CaseInsensitive )
               || contentType.startsWith( "text/plain", Qt::CaseInsensitive ) ) {
            //Article
            if ( maxHeadwordsToExpand && entries > maxHeadwordsToExpand ) {
              indexedWords.addSingleWord( refEntry.key.toStdU32String(), offsets[ i ].second );
            }
            else {
              indexedWords.addWord( refEntry.key.toStdU32String(), offsets[ i ].second );
            }

            wordCount += 1;

            quint64 pos = ( ( (quint64)refEntry.itemIndex ) << 32 ) + refEntry.binIndex;
            if ( articlesPos.find( pos ) == articlesPos.end() ) {
              articleCount += 1;
              articlesPos.insert( pos );
            }
          }
          else {
            indexedResources.addSingleWord( refEntry.key.toStdU32String(), offsets[ i ].second );
          }
        }
        sf.clearRefOffsets();

        // Build index

        {
          IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

          idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.indexRootOffset       = idxInfo.rootOffset;

          indexedWords.clear(); // Release memory -- no need for this data
        }

        {
          IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedResources, idx );

          idxHeader.resourceIndexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.resourceIndexRootOffset       = idxInfo.rootOffset;

          indexedResources.clear(); // Release memory -- no need for this data
        }

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        idxHeader.articleCount = articleCount;
        idxHeader.wordCount    = wordCount;

        auto langs = LangCoder::findLangIdPairFromPath( dictFiles[ 0 ] );

        idxHeader.langFrom = langs.first;
        idxHeader.langTo   = langs.second;

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }
      dictionaries.push_back( std::make_shared< SlobDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Slob dictionary initializing failed: %s, error: %s", fileName.c_str(), e.what() );
      continue;
    }
    catch ( ... ) {
      qWarning( "Slob dictionary initializing failed" );
      continue;
    }
  }
  return dictionaries;
}

} // namespace Slob
