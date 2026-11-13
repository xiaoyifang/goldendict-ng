/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "sounddir.hh"
#include "folding.hh"
#include "btreeidx.hh"
#include "chunkedstorage.hh"
#include "filetype.hh"
#include "htmlescape.hh"
#include "audiolink.hh"
#include "lsa.hh"
#include "globalbroadcaster.hh"
#include "text.hh"

#include "utils.hh"

#include <set>
#include <cstring>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>
#include <QDataStream>
#include <QSet>

namespace SoundDir {

using std::string;
using std::map;
using std::multimap;
using std::set;
using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

enum {
  Signature            = 0x58524453, // SDRX on little-endian, XRDS on big-endian
  CurrentFormatVersion = 5 + BtreeIndexing::FormatVersion + Folding::Version
};

#pragma pack( push, 1 )
struct IdxHeader
{
  uint32_t signature;             // First comes the signature, SDRX
  uint32_t formatVersion;         // File format version, is to be CurrentFormatVersion
  uint32_t soundsCount;           // Total number of sounds, for informative purposes only
  uint32_t chunksOffset;          // The offset to chunks' storage
  uint32_t dirTimestampsOffset;   // Offset to directory timestamps map
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}

/// Records stored in the chunks share a unified binary layout:
///
///   [ uint32 type ][ uint32 payloadSize ][ payload ]
///
/// The payload is a sequence of fields, their order defined by the type:
///
///   SoundFile: str path         -- relative path of the sound file
///   LsaEntry:  u32 vorbisOffset -- offset of the Ogg stream in the archive
///              u32 entryOffset  -- offset of the entry record in the archive
///              str name         -- entry name, the extension stripped
///              str path         -- relative path of the .lsa archive
///
/// A str is stored as [ uint32 size ][ size bytes of UTF-8 ], so neither a
/// terminator nor escaping is needed and embedded NUL characters are harmless.
///
/// Values use the host byte order, like the rest of the index file; the index
/// header signature makes a foreign-endianness index fail to open, which
/// forces a rebuild.
enum class RecordType : uint32_t {
  SoundFile = 0,
  LsaEntry  = 1
};

struct RecordWriter
{
  string buffer;

  void u32( uint32_t value )
  {
    buffer.append( reinterpret_cast< const char * >( &value ), sizeof( value ) );
  }

  void str( const string & value )
  {
    u32( static_cast< uint32_t >( value.size() ) );
    buffer.append( value );
  }
};

struct RecordReader
{
  const char * p   = nullptr;
  const char * end = nullptr;

  RecordReader() = default;

  RecordReader( const char * start, size_t size ): p( start ), end( start + size ) {}

  bool u32( uint32_t & value )
  {
    if ( static_cast< size_t >( end - p ) < sizeof( value ) ) {
      return false;
    }

    memcpy( &value, p, sizeof( value ) );
    p += sizeof( value );
    return true;
  }

  bool str( string & value )
  {
    uint32_t size = 0;

    if ( !u32( size ) || static_cast< size_t >( end - p ) < size ) {
      return false;
    }

    value.assign( p, size );
    p += size;
    return true;
  }
};

/// Appends a record of \p type with the given payload to the chunks storage
/// and returns its article offset.
uint32_t addRecord( ChunkedStorage::Writer & chunks, RecordType type, const string & payload )
{
  RecordWriter record;

  record.u32( static_cast< uint32_t >( type ) );
  record.u32( static_cast< uint32_t >( payload.size() ) );
  record.buffer.append( payload );

  const uint32_t articleOffset = chunks.startNewBlock();

  chunks.addToBlock( record.buffer.data(), record.buffer.size() );

  return articleOffset;
}

/// Reads the record header at \p recordStart and makes \p payload span the
/// payload only. Returns false if the record is malformed or truncated.
bool openRecord( const char * recordStart, const char * chunkEnd, RecordType & type, RecordReader & payload )
{
  if ( recordStart >= chunkEnd ) {
    return false;
  }

  RecordReader record( recordStart, static_cast< size_t >( chunkEnd - recordStart ) );

  uint32_t typeValue   = 0;
  uint32_t payloadSize = 0;

  if ( !record.u32( typeValue ) || !record.u32( payloadSize )
       || static_cast< size_t >( chunkEnd - record.p ) < payloadSize ) {
    return false;
  }

  type = static_cast< RecordType >( typeValue );

  payload = RecordReader( record.p, payloadSize );

  return true;
}

class SoundDirDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;
  ChunkedStorage::Reader chunks;
  QString iconFilename;

public:

  SoundDirDictionary( const string & id,
                      const string & name,
                      const string & indexFile,
                      const vector< string > & dictionaryFiles,
                      const QString & iconFilename_ );

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.soundsCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return getArticleCount();
  }

  sptr< Dictionary::DataRequest > getArticle( const std::u32string &,
                                              const vector< std::u32string > & alts,
                                              const std::u32string &,
                                              bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( const string & name ) override;

protected:

  void loadIcon() noexcept override;
  bool get_file_name( uint32_t articleOffset, QString & file_name );
};

SoundDirDictionary::SoundDirDictionary( const string & id,
                                        const string & name_,
                                        const string & indexFile,
                                        const vector< string > & dictionaryFiles,
                                        const QString & iconFilename_ ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() ),
  chunks( idx, idxHeader.chunksOffset ),
  iconFilename( iconFilename_ )
{
  dictionaryName = name_;

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );
}

sptr< Dictionary::DataRequest > SoundDirDictionary::getArticle( const std::u32string & word,
                                                                const vector< std::u32string > & alts,
                                                                const std::u32string &,
                                                                bool ignoreDiacritics )
{
  vector< WordArticleLink > chain = findArticles( word, ignoreDiacritics );

  for ( const auto & alt : alts ) {
    /// Make an additional query for each alt

    vector< WordArticleLink > altChain = findArticles( alt, ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  // maps to the chain number
  multimap< std::u32string, unsigned > mainArticles, alternateArticles;

  set< uint32_t > articlesIncluded; // Some synonims make it that the articles
                                    // appear several times. We combat this
                                    // by only allowing them to appear once.

  std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics ) {
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
  }

  for ( unsigned x = 0; x < chain.size(); ++x ) {
    if ( articlesIncluded.find( chain[ x ].articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    std::u32string headwordStripped = Folding::applySimpleCaseOnly( chain[ x ].word );
    if ( ignoreDiacritics ) {
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
    }

    multimap< std::u32string, unsigned > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( std::pair( Folding::applySimpleCaseOnly( chain[ x ].word ), x ) );

    articlesIncluded.insert( chain[ x ].articleOffset );
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such word
  }

  string result;

  multimap< std::u32string, uint32_t >::const_iterator i;

  string displayedName;

  result += "<div class=\"audio-play\">";

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    uint32_t address = chain[ i->second ].articleOffset;

    if ( mainArticles.size() + alternateArticles.size() <= 1 ) {
      displayedName = chain[ i->second ].word;
    }
    else {
      QString name;
      if ( !get_file_name( address, name ) ) {
        // Bad address
        continue;
      }
      displayedName = name.toStdString();
    }

    result += "<div class=\"audio-play-item\">";
    auto _displayName = Html::escape( displayedName );
    QString file_name;
    if ( !get_file_name( address, file_name ) ) {
      // Bad address
      file_name = QString::fromStdString( _displayName );
    }
    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    auto path = Utils::Url::ensureLeadingSlash( QString( "%1/%2" ).arg( QString::number( address ), file_name ) );
    url.setPath( path );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    result += addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"( class="audio-play-icon"></a>)";
    result += "<a href=" + ref + ">" + _displayName + "</a>";
    result += "</div>";
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    uint32_t address = chain[ i->second ].articleOffset;

    if ( mainArticles.size() + alternateArticles.size() <= 1 ) {
      displayedName = chain[ i->second ].word;
    }
    else {
      QString name;
      if ( !get_file_name( address, name ) ) {
        // Bad address
        continue;
      }
      displayedName = name.toStdString();
    }

    result += "<div class=\"audio-play-item\">";
    auto _displayName = Html::escape( displayedName );
    QString file_name;
    if ( !get_file_name( address, file_name ) ) {
      // Bad address
      file_name = QString::fromStdString( _displayName );
    }
    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    auto path = Utils::Url::ensureLeadingSlash( QString( "%1/%2" ).arg( QString::number( address ), file_name ) );
    url.setPath( path );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"( class="audio-play-icon"></a>)";
    result += "<a href=" + ref + ">" + _displayName + "</a>";
    result += "</div>";
  }

  result += "</div>";

  auto ret = std::make_shared< Dictionary::DataRequestInstant >( true );

  ret->appendString( result );

  return ret;
}

void SoundDirDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !iconFilename.isEmpty() ) {
    const QFileInfo fInfo( QDir( Config::getConfigDir() ), iconFilename );
    if ( fInfo.isFile() ) {
      loadIconFromFilePath( fInfo.absoluteFilePath() );
    }
  }
  if ( dictionaryIcon.isNull() ) {
    dictionaryIcon = QIcon( ":/icons/sounddir.svg" );
  }
  dictionaryIconLoaded = true;
}

bool SoundDirDictionary::get_file_name( uint32_t articleOffset, QString & file_name )
{
  vector< char > chunk;
  char * recordData;

  try {
    QMutexLocker _( &idxMutex );

    recordData = chunks.getBlock( articleOffset, chunk );

    if ( chunk.empty() || recordData >= &chunk.front() + chunk.size() ) {
      // chunks reader thinks it's okay since zero-sized records can exist,
      // but we don't allow that.
      throw ChunkedStorage::exAddressOutOfRange();
    }
  }
  catch ( ChunkedStorage::exAddressOutOfRange & ) {
    return false; // No such resource
  }

  RecordType type;
  RecordReader payload;

  if ( !openRecord( recordData, &chunk.front() + chunk.size(), type, payload ) ) {
    return false; // Malformed record
  }

  if ( type == RecordType::LsaEntry ) {
    uint32_t vorbisOffset = 0, entryOffset = 0;
    string entryName;

    if ( !payload.u32( vorbisOffset ) || !payload.u32( entryOffset ) || !payload.str( entryName ) ) {
      return false; // Malformed record
    }

    file_name = QString::fromStdString( entryName );
    return true;
  }

  string relPath;

  if ( type != RecordType::SoundFile || !payload.str( relPath ) ) {
    return false; // Malformed record
  }

  file_name = QString::fromStdString( relPath );
  return true;
}

sptr< Dictionary::DataRequest > SoundDirDictionary::getResource( const string & name )

{
  bool isNumber = false;
  uint32_t articleOffset;

  const auto _name       = QString::fromStdString( name );
  const qint64 sep_index = _name.indexOf( '/' );
  if ( sep_index > 0 ) {
    const auto number = _name.left( sep_index );
    articleOffset     = number.toULong( &isNumber );
  }
  else {
    articleOffset = QString::fromUtf8( name.c_str() ).toULong( &isNumber );
  }


  if ( !isNumber ) {
    // The name is not the internal offset-based one but a plain file or
    // entry name, e.g. when the sound is requested by another dictionary
    // ( a DSL fallback or the #SOUND_DICTIONARY directive ). Resolve it
    // through the index.
    string strippedName( name );

    if ( Filetype::isNameOfSound( strippedName ) ) {
      const auto dotPos = strippedName.rfind( '.' );
      if ( dotPos != string::npos ) {
        strippedName.erase( dotPos );
      }
    }

    vector< WordArticleLink > chain = findArticles( Text::toUtf32( strippedName ) );

    if ( chain.empty() ) {
      return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
    }

    articleOffset = chain[ 0 ].articleOffset;
  }

  vector< char > chunk;
  char * recordData;

  try {
    QMutexLocker _( &idxMutex );

    recordData = chunks.getBlock( articleOffset, chunk );

    if ( chunk.empty() || recordData >= &chunk.front() + chunk.size() ) {
      // chunks reader thinks it's okay since zero-sized records can exist,
      // but we don't allow that.
      throw ChunkedStorage::exAddressOutOfRange();
    }
  }
  catch ( ChunkedStorage::exAddressOutOfRange & ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
  }

  RecordType type;
  RecordReader payload;

  if ( !openRecord( recordData, &chunk.front() + chunk.size(), type, payload ) ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // Malformed record
  }

  const QDir dir( QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() ) );

  if ( type == RecordType::LsaEntry ) {
    // The record references an entry inside a Lingvo sound archive (.lsa)

    uint32_t vorbisOffset = 0, entryOffset = 0;
    string entryName, relLsaPath;

    if ( !payload.u32( vorbisOffset ) || !payload.u32( entryOffset ) || !payload.str( entryName )
         || !payload.str( relLsaPath ) ) {
      return std::make_shared< Dictionary::DataRequestInstant >( false ); // Malformed record
    }

    const QString lsaPath =
      QDir::toNativeSeparators( dir.filePath( QString::fromStdString( relLsaPath ) ) );

    auto dr = std::make_shared< Dictionary::DataRequestInstant >( true );

    try {
      Lsa::loadLsaSound( lsaPath.toStdString(), entryOffset, vorbisOffset, dr->getData() );
    }
    catch ( std::exception & ) {
      return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
    }

    return dr;
  }

  string relPath;

  if ( type != RecordType::SoundFile || !payload.str( relPath ) ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // Malformed record
  }

  const QString fileName = QDir::toNativeSeparators( dir.filePath( QString::fromStdString( relPath ) ) );

  // Now try loading that file

  try {
    File::Index f( fileName.toStdString(), QIODevice::ReadOnly );

    sptr< Dictionary::DataRequestInstant > dr = std::make_shared< Dictionary::DataRequestInstant >( true );

    vector< char > & data = dr->getData();

    f.seekEnd();

    data.resize( f.tell() );

    f.rewind();
    f.read( &data.front(), data.size() );

    return dr;
  }
  catch ( File::Ex & ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
  }
}

void addDir( const QDir & dir, IndexedWords & indexedWords, uint32_t & soundsCount, ChunkedStorage::Writer & chunks )
{
  QDirIterator it( dir.path(),
                   QDir::Files | QDir::NoSymLinks, // We only care about files
                   QDirIterator::Subdirectories ); // Recurse into subdirectories

  while ( it.hasNext() ) {
    it.next();
    const QFileInfo & fileInfo = it.fileInfo();

    if ( Filetype::isNameOfSound( fileInfo.fileName().toUtf8().data() ) ) {
      // Add this sound to index
      string fileName = dir.relativeFilePath( fileInfo.filePath() ).toUtf8().data();

      RecordWriter payload;
      payload.str( fileName );

      const uint32_t articleOffset = addRecord( chunks, RecordType::SoundFile, payload.buffer );

      std::u32string name = fileInfo.fileName().toStdU32String();

      const std::u32string::size_type pos = name.rfind( L'.' );

      if ( pos != std::u32string::npos ) {
        name.erase( pos );
      }

      indexedWords.addWord( name, articleOffset );

      ++soundsCount;
    }
  }
}

/// Adds entries of Lingvo sound archives (.lsa) found inside the directory.
/// Only archives with entries named after real words ("universal" ones) are
/// of any use here; archives which name their sounds with internal ids are
/// paired with a specific DSL dictionary and must stay beside it instead.
void addLsaArchives( const QDir & dir,
                     IndexedWords & indexedWords,
                     uint32_t & soundsCount,
                     ChunkedStorage::Writer & chunks )
{
  QDirIterator it( dir.path(),
                   QDir::Files | QDir::NoSymLinks, // We only care about files
                   QDirIterator::Subdirectories ); // Recurse into subdirectories

  while ( it.hasNext() ) {
    it.next();

    const std::string filePath = it.filePath().toStdString();

    if ( !Utils::endsWithIgnoreCase( filePath, ".lsa" ) ) {
      continue;
    }

    std::vector< Lsa::LsaEntryInfo > entries;
    uint32_t vorbisOffset = 0;

    if ( !Lsa::parseLsaEntries( filePath, entries, vorbisOffset ) ) {
      continue; // Not a valid LSA archive
    }

    const string relLsaPath = dir.relativeFilePath( it.filePath() ).toUtf8().data();

    for ( const auto & entry : entries ) {
      if ( entry.name.empty() ) {
        continue;
      }

      RecordWriter payload;
      payload.u32( vorbisOffset );
      payload.u32( entry.offset );
      payload.str( entry.name );
      payload.str( relLsaPath );

      const uint32_t articleOffset = addRecord( chunks, RecordType::LsaEntry, payload.buffer );

      indexedWords.addWord( Text::toUtf32( entry.name ), articleOffset );

      ++soundsCount;
    }
  }
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( const Config::SoundDirs & soundDirs,
                                                      const string & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & soundDir : soundDirs ) {
    QDir dir( soundDir.path );

    if ( !dir.exists() ) {
      continue; // No such dir, no dictionary then
    }

    vector< string > dictFiles( 1, QDir::toNativeSeparators( dir.canonicalPath() ).toStdString() );

    dictFiles.push_back( "SoundDir" ); // A mixin

    string dictId = Dictionary::makeDictionaryId( dictFiles );

    dictFiles.pop_back(); // Remove mixin

    string indexFile = indicesDir + dictId;

    bool rebuildNeeded;

    if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
      rebuildNeeded = true;
    }
    else {
      rebuildNeeded = false; // Assume it's fine
      try {
        File::Index idx( indexFile, QIODevice::ReadOnly );
        IdxHeader header = idx.read< IdxHeader >();
        idx.seek( header.dirTimestampsOffset );

        // Reading the data for QDataStream
        vector< char > data;
        data.resize( idx.size() - header.dirTimestampsOffset );
        idx.read( &data.front(), data.size() );
        QByteArray timestampData = QByteArray::fromRawData( data.data(), data.size() );
        QDataStream stream( &timestampData, QIODevice::ReadOnly );

        QMap< QString, QDateTime > storedTimestamps;
        stream >> storedTimestamps;

        QSet< QString > onDiskDirs;

        // Check root dir
        QFileInfo rootInfo( dir.path() );
        QString rootRelPath = ".";
        onDiskDirs.insert( rootRelPath );
        if ( !storedTimestamps.contains( rootRelPath )
             || storedTimestamps.value( rootRelPath ) != rootInfo.lastModified() ) {
          rebuildNeeded = true;
        }

        // Check subdirs
        if ( !rebuildNeeded ) {
          QDirIterator it( dir.path(),
                           QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                           QDirIterator::Subdirectories );
          while ( it.hasNext() ) {
            it.next();
            QString relPath = dir.relativeFilePath( it.filePath() );
            onDiskDirs.insert( relPath );
            if ( !storedTimestamps.contains( relPath )
                 || storedTimestamps.value( relPath ) != it.fileInfo().lastModified() ) {
              rebuildNeeded = true;
              break;
            }
          }
        }

        // Check for deleted directories
        if ( !rebuildNeeded && onDiskDirs.size() != storedTimestamps.size() ) {
          rebuildNeeded = true;
        }
      }
      catch ( std::exception & e ) {
        qWarning( "Could not verify sounddir index for %s, forcing rebuild. Error: %s",
                  soundDir.path.toUtf8().constData(),
                  e.what() );
        rebuildNeeded = true;
      }
    }

    if ( rebuildNeeded ) {
      // Building the index

      qDebug() << "Sounds: Building the index for directory: " << soundDir.path;

      initializing.indexingDictionary( soundDir.name.toUtf8().data() );

      File::Index idx( indexFile, QIODevice::WriteOnly );

      IdxHeader idxHeader = {};

      // We write a dummy header first. At the end of the process the header
      // will be rewritten with the right values.

      idx.write( idxHeader );

      IndexedWords indexedWords;

      ChunkedStorage::Writer chunks( idx );

      uint32_t soundsCount = 0; // Header's one is packed, we can't ref it

      addDir( dir, indexedWords, soundsCount, chunks );

      addLsaArchives( dir, indexedWords, soundsCount, chunks );

      idxHeader.soundsCount = soundsCount;

      // Finish with the chunks

      idxHeader.chunksOffset = chunks.finish();

      // Build the index

      IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

      idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
      idxHeader.indexRootOffset       = idxInfo.rootOffset;

      // Write directory timestamps
      idxHeader.dirTimestampsOffset = idx.tell();
      QMap< QString, QDateTime > dirTimestamps;
      QDirIterator it( dir.path(),
                       QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                       QDirIterator::Subdirectories );
      while ( it.hasNext() ) {
        it.next();
        dirTimestamps.insert( dir.relativeFilePath( it.filePath() ), it.fileInfo().lastModified() );
      }
      // Also add the root dir itself
      dirTimestamps.insert( QString( "." ), QFileInfo( dir.path() ).lastModified() );

      // Now write the map to the file
      QByteArray timestampData;
      QDataStream stream( &timestampData, QIODevice::WriteOnly );
      stream << dirTimestamps;
      idx.write( timestampData.constData(), timestampData.size() );


      // That concludes it. Update the header.

      idxHeader.signature     = Signature;
      idxHeader.formatVersion = CurrentFormatVersion;

      idx.rewind();

      idx.write( &idxHeader, sizeof( idxHeader ) );
    }

    auto dict = std::make_shared< SoundDirDictionary >( dictId,
                                                        soundDir.name.toUtf8().data(),
                                                        indexFile,
                                                        dictFiles,
                                                        soundDir.iconFilename );

    GlobalBroadcaster::instance()->addSoundDirDictId( QString::fromStdString( dictId ) );

    dictionaries.push_back( dict );
  }

  return dictionaries;
}


} // namespace SoundDir
