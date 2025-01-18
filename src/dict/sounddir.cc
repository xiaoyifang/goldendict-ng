/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "sounddir.hh"
#include "folding.hh"
#include "text.hh"
#include "btreeidx.hh"
#include "chunkedstorage.hh"
#include "filetype.hh"
#include "htmlescape.hh"
#include "audiolink.hh"

#include "utils.hh"

#include <set>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

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
  CurrentFormatVersion = 1 + BtreeIndexing::FormatVersion + Folding::Version
};

#pragma pack( push, 1 )
struct IdxHeader
{
  uint32_t signature;             // First comes the signature, SDRX
  uint32_t formatVersion;         // File format version, is to be CurrentFormatVersion
  uint32_t soundsCount;           // Total number of sounds, for informative purposes only
  uint32_t chunksOffset;          // The offset to chunks' storage
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

class SoundDirDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;
  ChunkedStorage::Reader chunks;
  QString iconFilename;

public:

  SoundDirDictionary( string const & id,
                      string const & name,
                      string const & indexFile,
                      vector< string > const & dictionaryFiles,
                      QString const & iconFilename_ );

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.soundsCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return getArticleCount();
  }

  sptr< Dictionary::DataRequest > getArticle( std::u32string const &,
                                              vector< std::u32string > const & alts,
                                              std::u32string const &,
                                              bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

protected:

  void loadIcon() noexcept override;
  bool get_file_name( uint32_t articleOffset, QString & file_name );
};

SoundDirDictionary::SoundDirDictionary( string const & id,
                                        string const & name_,
                                        string const & indexFile,
                                        vector< string > const & dictionaryFiles,
                                        QString const & iconFilename_ ):
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

sptr< Dictionary::DataRequest > SoundDirDictionary::getArticle( std::u32string const & word,
                                                                vector< std::u32string > const & alts,
                                                                std::u32string const &,
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
  vector< char > chunk;
  char * nameBlock;

  result += "<div class=\"audio-play\">";

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    uint32_t address = chain[ i->second ].articleOffset;

    if ( mainArticles.size() + alternateArticles.size() <= 1 ) {
      displayedName = chain[ i->second ].word;
    }
    else {
      try {
        QMutexLocker _( &idxMutex );
        nameBlock = chunks.getBlock( address, chunk );

        if ( nameBlock >= &chunk.front() + chunk.size() ) {
          // chunks reader thinks it's okay since zero-sized records can exist,
          // but we don't allow that.
          throw ChunkedStorage::exAddressOutOfRange();
        }

        chunk.back()  = 0; // It must end with 0 anyway, but just in case
        displayedName = string( nameBlock );
      }
      catch ( ChunkedStorage::exAddressOutOfRange & ) {
        // Bad address
        continue;
      }
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

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
    result += "<a href=" + ref + ">" + _displayName + "</a>";
    result += "</div>";
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    uint32_t address = chain[ i->second ].articleOffset;

    if ( mainArticles.size() + alternateArticles.size() <= 1 ) {
      displayedName = chain[ i->second ].word;
    }
    else {
      try {
        QMutexLocker _( &idxMutex );
        nameBlock = chunks.getBlock( address, chunk );

        if ( nameBlock >= &chunk.front() + chunk.size() ) {
          // chunks reader thinks it's okay since zero-sized records can exist,
          // but we don't allow that.
          throw ChunkedStorage::exAddressOutOfRange();
        }

        chunk.back()  = 0; // It must end with 0 anyway, but just in case
        displayedName = string( nameBlock );
      }
      catch ( ChunkedStorage::exAddressOutOfRange & ) {
        // Bad address
        continue;
      }
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

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
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
      loadIconFromFile( fInfo.absoluteFilePath(), true );
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
  char * articleData;

  try {
    QMutexLocker _( &idxMutex );

    articleData = chunks.getBlock( articleOffset, chunk );

    if ( articleData >= &chunk.front() + chunk.size() ) {
      // chunks reader thinks it's okay since zero-sized records can exist,
      // but we don't allow that.
      throw ChunkedStorage::exAddressOutOfRange();
    }
  }
  catch ( ChunkedStorage::exAddressOutOfRange & ) {
    return false; // No such resource
  }

  chunk.back() = 0; // It must end with 0 anyway, but just in case
  file_name    = QString::fromUtf8( articleData );
  return true;
}

sptr< Dictionary::DataRequest > SoundDirDictionary::getResource( string const & name )

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
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
  }

  QString file_name;
  if ( !get_file_name( articleOffset, file_name ) ) {
    // Bad address
    return std::make_shared< Dictionary::DataRequestInstant >( false );
  }

  const QDir dir( QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() ) );

  const QString fileName = QDir::toNativeSeparators( dir.filePath( file_name ) );

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

void addDir( QDir const & baseDir,
             QDir const & dir,
             IndexedWords & indexedWords,
             uint32_t & soundsCount,
             ChunkedStorage::Writer & chunks )
{
  const QFileInfoList entries = dir.entryInfoList( QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot );

  for ( QFileInfoList::const_iterator i = entries.constBegin(); i != entries.constEnd(); ++i ) {
    if ( i->isDir() ) {
      addDir( baseDir, QDir( i->absoluteFilePath() ), indexedWords, soundsCount, chunks );
    }
    else if ( Filetype::isNameOfSound( i->fileName().toUtf8().data() ) ) {
      // Add this sound to index
      string fileName = baseDir.relativeFilePath( i->filePath() ).toUtf8().data();

      const uint32_t articleOffset = chunks.startNewBlock();
      chunks.addToBlock( fileName.c_str(), fileName.size() + 1 );

      std::u32string name = i->fileName().toStdU32String();

      const std::u32string::size_type pos = name.rfind( L'.' );

      if ( pos != std::u32string::npos ) {
        name.erase( pos );
      }

      indexedWords.addWord( name, articleOffset );

      ++soundsCount;
    }
  }
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( Config::SoundDirs const & soundDirs,
                                                      string const & indicesDir,
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

    // Check if the soundDir and its subdirs' modification date changed, that means the user modified the sound files inside

    bool soundDirModified = false;
    {
      QDateTime indexFileModifyTime = QFileInfo( QString::fromStdString( indexFile ) ).lastModified();
      QDirIterator it( dir.path(),
                       QDir::AllDirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                       QDirIterator::Subdirectories );

      QDeadlineTimer deadline( 4000 );
      while ( it.hasNext() && !deadline.hasExpired() ) {
        it.next();
        if ( it.fileInfo().lastModified() > indexFileModifyTime ) {
          soundDirModified = true;
          break;
        }
      }
      if ( deadline.hasExpired() ) {
        qDebug() << "SoundDir modification scanning timed out.";
      }
    }

    if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) || soundDirModified ) {
      // Building the index

      qDebug() << "Sounds: Building the index for directory: " << soundDir.path;

      initializing.indexingDictionary( soundDir.name.toUtf8().data() );

      File::Index idx( indexFile, QIODevice::WriteOnly );

      IdxHeader idxHeader;

      memset( &idxHeader, 0, sizeof( idxHeader ) );

      // We write a dummy header first. At the end of the process the header
      // will be rewritten with the right values.

      idx.write( idxHeader );

      IndexedWords indexedWords;

      ChunkedStorage::Writer chunks( idx );

      uint32_t soundsCount = 0; // Header's one is packed, we can't ref it

      addDir( dir, dir, indexedWords, soundsCount, chunks );

      idxHeader.soundsCount = soundsCount;

      // Finish with the chunks

      idxHeader.chunksOffset = chunks.finish();

      // Build the index

      IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

      idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
      idxHeader.indexRootOffset       = idxInfo.rootOffset;

      // That concludes it. Update the header.

      idxHeader.signature     = Signature;
      idxHeader.formatVersion = CurrentFormatVersion;

      idx.rewind();

      idx.write( &idxHeader, sizeof( idxHeader ) );
    }

    dictionaries.push_back( std::make_shared< SoundDirDictionary >( dictId,
                                                                    soundDir.name.toUtf8().data(),
                                                                    indexFile,
                                                                    dictFiles,
                                                                    soundDir.iconFilename ) );
  }

  return dictionaries;
}


} // namespace SoundDir
