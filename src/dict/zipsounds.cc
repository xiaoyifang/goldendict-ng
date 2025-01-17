/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "zipsounds.hh"
#include "dictfile.hh"
#include "folding.hh"
#include "text.hh"
#include "btreeidx.hh"

#include "audiolink.hh"
#include "indexedzip.hh"
#include "filetype.hh"
#include "chunkedstorage.hh"
#include "htmlescape.hh"

#include <set>
#include <string>
#include <QFile>
#include <QDir>


#include "utils.hh"

namespace ZipSounds {

using std::string;
using std::map;
using std::multimap;
using std::set;
using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

DEF_EX( exInvalidData, "Invalid data encountered", Dictionary::Ex )

enum {
  Signature            = 0x5350495a, // ZIPS on little-endian, SPIZ on big-endian
  CurrentFormatVersion = 6 + BtreeIndexing::FormatVersion + BtreeIndexing::ZipParseLogicVersion
};

#pragma pack( push, 1 )
struct IdxHeader
{
  uint32_t signature;             // First comes the signature, ZIPS
  uint32_t formatVersion;         // File format version, currently 1.
  uint32_t soundsCount;           // Total number of sounds, for informative purposes only
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
  uint32_t chunksOffset; // The offset to chunks' storage
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

std::u32string stripExtension( string const & str )
{
  std::u32string name;
  try {
    name = Text::toUtf32( str );
  }
  catch ( Text::exCantDecode & ) {
    return name;
  }

  if ( Filetype::isNameOfSound( str ) ) {
    std::u32string::size_type pos = name.rfind( L'.' );
    if ( pos != std::u32string::npos ) {
      name.erase( pos );
    }

    // Strip spaces at the end of name
    string::size_type n = name.length();
    while ( n && name.at( n - 1 ) == L' ' ) {
      n--;
    }

    if ( n != name.length() ) {
      name.erase( n );
    }
  }
  return name;
}

class ZipSoundsDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;
  sptr< ChunkedStorage::Reader > chunks;
  IndexedZip zipsFile;

public:

  ZipSoundsDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  string getName() noexcept override;


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
};

ZipSoundsDictionary::ZipSoundsDictionary( string const & id,
                                          string const & indexFile,
                                          vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() )
{
  chunks = std::shared_ptr< ChunkedStorage::Reader >( new ChunkedStorage::Reader( idx, idxHeader.chunksOffset ) );

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  QString zipName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  zipsFile.openZipFile( zipName );
  zipsFile.openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );
}

string ZipSoundsDictionary::getName() noexcept
{
  string result = Utils::Fs::basename( getDictionaryFilenames()[ 0 ] );

  // Strip the extension
  result.erase( result.rfind( '.' ) );

  return result;
}

sptr< Dictionary::DataRequest > ZipSoundsDictionary::getArticle( std::u32string const & word,
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

  multimap< std::u32string, uint32_t > mainArticles, alternateArticles;

  set< uint32_t > articlesIncluded; // Some synonims make it that the articles
                                    // appear several times. We combat this
                                    // by only allowing them to appear once.

  std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics ) {
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
  }

  for ( auto & x : chain ) {
    if ( articlesIncluded.find( x.articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    std::u32string headwordStripped = Folding::applySimpleCaseOnly( x.word );
    if ( ignoreDiacritics ) {
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
    }

    multimap< std::u32string, uint32_t > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( std::pair( Folding::applySimpleCaseOnly( x.word ), x.articleOffset ) );

    articlesIncluded.insert( x.articleOffset );
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such word
  }

  string result;

  multimap< std::u32string, uint32_t >::const_iterator i;

  result += "<div class=\"audio-play\">";

  vector< char > chunk;
  char * nameBlock;

  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    try {
      QMutexLocker _( &idxMutex );
      nameBlock = chunks->getBlock( i->second, chunk );

      if ( nameBlock >= &chunk.front() + chunk.size() ) {
        // chunks reader thinks it's okay since zero-sized records can exist,
        // but we don't allow that.
        throw ChunkedStorage::exAddressOutOfRange();
      }
    }
    catch ( ChunkedStorage::exAddressOutOfRange & ) {
      // Bad address
      continue;
    }

    uint16_t sz;
    memcpy( &sz, nameBlock, sizeof( uint16_t ) );
    nameBlock += sizeof( uint16_t );

    string name( nameBlock, sz );
    nameBlock += sz;

    string displayedName =
      mainArticles.size() + alternateArticles.size() > 1 ? name : Text::toUtf8( stripExtension( name ) );

    result += "<div class=\"audio-play-item\">";

    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( name.c_str() ) ) );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    result += addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
    result += "<a href=" + ref + ">" + Html::escape( displayedName ) + "</a>";
    result += "</div>";
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    try {
      QMutexLocker _( &idxMutex );
      nameBlock = chunks->getBlock( i->second, chunk );

      if ( nameBlock >= &chunk.front() + chunk.size() ) {
        // chunks reader thinks it's okay since zero-sized records can exist,
        // but we don't allow that.
        throw ChunkedStorage::exAddressOutOfRange();
      }
    }
    catch ( ChunkedStorage::exAddressOutOfRange & ) {
      // Bad address
      continue;
    }

    uint16_t sz;
    memcpy( &sz, nameBlock, sizeof( uint16_t ) );
    nameBlock += sizeof( uint16_t );

    string name( nameBlock, sz );
    nameBlock += sz;

    string displayedName =
      mainArticles.size() + alternateArticles.size() > 1 ? name : Text::toUtf8( stripExtension( name ) );

    result += "<div class=\"audio-play-item\">";

    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( name.c_str() ) ) );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
    result += "<a href=" + ref + ">" + Html::escape( displayedName ) + "</a>";
    result += "</div>";
  }

  result += "</div>";

  auto ret = std::make_shared< Dictionary::DataRequestInstant >( true );
  ret->appendString( result );
  return ret;
}

sptr< Dictionary::DataRequest > ZipSoundsDictionary::getResource( string const & name )

{
  // Remove extension for sound files (like in sound dirs)

  std::u32string strippedName = stripExtension( name );

  vector< WordArticleLink > chain = findArticles( strippedName );

  if ( chain.empty() ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
  }

  // Find sound

  uint32_t dataOffset = 0;
  for ( int x = chain.size() - 1; x >= 0; x-- ) {
    vector< char > chunk;
    char * nameBlock = chunks->getBlock( chain[ x ].articleOffset, chunk );

    uint16_t sz;
    memcpy( &sz, nameBlock, sizeof( uint16_t ) );
    nameBlock += sizeof( uint16_t );

    string fileName( nameBlock, sz );
    nameBlock += sz;

    memcpy( &dataOffset, nameBlock, sizeof( uint32_t ) );

    if ( name.compare( fileName ) == 0 ) {
      break;
    }
  }

  sptr< Dictionary::DataRequestInstant > dr = std::make_shared< Dictionary::DataRequestInstant >( true );

  if ( zipsFile.loadFile( dataOffset, dr->getData() ) ) {
    return dr;
  }

  return std::make_shared< Dictionary::DataRequestInstant >( false );
}

void ZipSoundsDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  // Remove the extension
  fileName.chop( 4 );

  if ( !loadIconFromFile( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/zipsound.svg" );
  }

  dictionaryIconLoaded = true;
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  (void)initializing;
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    /// Only allow .zips extension
    if ( !Utils::endsWithIgnoreCase( fileName, ".zips" ) ) {
      continue;
    }

    try {
      vector< string > dictFiles( 1, fileName );
      string dictId    = Dictionary::makeDictionaryId( dictFiles );
      string indexFile = indicesDir + dictId;

      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        qDebug( "Zips: Building the index for dictionary: %s", fileName.c_str() );

        File::Index idx( indexFile, QIODevice::WriteOnly );
        IdxHeader idxHeader;

        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        IndexedWords names, zipFileNames;
        ChunkedStorage::Writer chunks( idx );
        quint32 namesCount = 0;

        IndexedZip zipFile;
        if ( zipFile.openZipFile( QDir::fromNativeSeparators( fileName.c_str() ) ) ) {
          zipFile.indexFile( zipFileNames, &namesCount );
        }

        if ( !zipFileNames.empty() ) {
          for ( auto & zipFileName : zipFileNames ) {
            vector< WordArticleLink > links = zipFileName.second;
            for ( auto & link : links ) {
              // Save original name

              uint32_t offset = chunks.startNewBlock();
              uint16_t sz     = link.word.size();
              chunks.addToBlock( &sz, sizeof( uint16_t ) );
              chunks.addToBlock( link.word.c_str(), sz );
              chunks.addToBlock( &link.articleOffset, sizeof( uint32_t ) );

              // Remove extension for sound files (like in sound dirs)

              std::u32string word = stripExtension( link.word );
              if ( !word.empty() ) {
                names.addWord( word, offset );
              }
            }
          }

          // Finish with the chunks

          idxHeader.chunksOffset = chunks.finish();

          // Build the resulting zip file index

          IndexInfo idxInfo = BtreeIndexing::buildIndex( names, idx );

          // That concludes it. Update the header.

          idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
          idxHeader.indexRootOffset       = idxInfo.rootOffset;

          idxHeader.signature     = Signature;
          idxHeader.formatVersion = CurrentFormatVersion;

          idxHeader.soundsCount = namesCount;

          idx.rewind();

          idx.write( &idxHeader, sizeof( idxHeader ) );
        }
        else {
          idx.close();
          QFile::remove( QDir::fromNativeSeparators( indexFile.c_str() ) );
          throw exInvalidData();
        }
      }

      dictionaries.push_back( std::make_shared< ZipSoundsDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Zipped sounds pack reading failed: %s, error: %s", fileName.c_str(), e.what() );
    }
  }

  return dictionaries;
}

} // namespace ZipSounds
