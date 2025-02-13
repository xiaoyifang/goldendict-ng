/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictdfiles.hh"
#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include "dictzip.hh"
#include "htmlescape.hh"
#include "langcoder.hh"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
#include <wctype.h>
#include <stdlib.h>
#include "ftshelpers.hh"
#include <QDir>
#include <QUrl>


#include <QRegularExpression>

namespace DictdFiles {

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::string;
using std::vector;
using std::list;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

using Dictionary::exCantReadFile;
DEF_EX( exFailedToReadLineFromIndex, "Failed to read line from index file", Dictionary::Ex )
DEF_EX( exMalformedIndexFileLine, "Malformed index file line encountered", Dictionary::Ex )
DEF_EX( exInvalidBase64, "Invalid base64 sequence encountered", Dictionary::Ex )
DEF_EX_STR( exDictzipError, "DICTZIP error", Dictionary::Ex )

enum {
  Signature            = 0x58444344, // DCDX on little-endian, XDCD on big-endian
  CurrentFormatVersion = 5 + BtreeIndexing::FormatVersion + Folding::Version
};

#pragma pack( push, 1 )

struct IdxHeader
{
  uint32_t signature;             // First comes the signature, DCDX
  uint32_t formatVersion;         // File format version (CurrentFormatVersion)
  uint32_t wordCount;             // Total number of words
  uint32_t articleCount;          // Total number of articles
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
  uint32_t langFrom; // Source language
  uint32_t langTo;   // Target language
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

class DictdDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx, indexFile; // The later is .index file
  IdxHeader idxHeader;
  dictData * dz;
  QMutex indexFileMutex, dzMutex;

public:

  DictdDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  ~DictdDictionary();

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.articleCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return idxHeader.wordCount;
  }

  void loadIcon() noexcept override;

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

  QString const & getDescription() override;

  sptr< Dictionary::DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( Config::FullTextSearch const & fts ) override
  {
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else {
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "DICTD", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }
};

DictdDictionary::DictdDictionary( string const & id,
                                  string const & indexFile,
                                  vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  indexFile( dictionaryFiles[ 0 ], QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() )
{

  // Read the dictionary name
  idx.seek( sizeof( idxHeader ) );

  idx.readU32SizeAndData<>( dictionaryName );

  // Open the .dict file

  DZ_ERRORS error;
  dz = dict_data_open( dictionaryFiles[ 1 ].c_str(), &error, 0 );

  if ( !dz ) {
    throw exDictzipError( string( dz_error_str( error ) ) + "(" + getDictionaryFilenames()[ 1 ] + ")" );
  }

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

DictdDictionary::~DictdDictionary()
{
  if ( dz ) {
    dict_data_close( dz );
  }
}

string nameFromFileName( string const & indexFileName )
{
  if ( indexFileName.empty() ) {
    return string();
  }

  char const * sep = strrchr( indexFileName.c_str(), Utils::Fs::separator() );

  if ( !sep ) {
    sep = indexFileName.c_str();
  }

  char const * dot = strrchr( sep, '.' );

  if ( !dot ) {
    dot = indexFileName.c_str() + indexFileName.size();
  }

  return string( sep + 1, dot - sep - 1 );
}

void DictdDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( QString::fromStdString( getDictionaryFilenames()[ 0 ] ) );

  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_dictd.png" );
  }

  dictionaryIconLoaded = true;
}

uint32_t decodeBase64( string const & str )
{
  static char const digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  uint32_t number = 0;

  for ( char const * next = str.c_str(); *next; ++next ) {
    char const * d = strchr( digits, *next );

    if ( !d ) {
      throw exInvalidBase64();
    }

    number = number * 64 + ( d - digits );
  }

  return number;
}

sptr< Dictionary::DataRequest > DictdDictionary::getArticle( std::u32string const & word,
                                                             vector< std::u32string > const & alts,
                                                             std::u32string const &,
                                                             bool ignoreDiacritics )

{
  try {
    vector< WordArticleLink > chain = findArticles( word, ignoreDiacritics );

    for ( const auto & alt : alts ) {
      /// Make an additional query for each alt

      vector< WordArticleLink > altChain = findArticles( alt, ignoreDiacritics );

      chain.insert( chain.end(), altChain.begin(), altChain.end() );
    }

    multimap< std::u32string, string > mainArticles, alternateArticles;

    set< uint32_t > articlesIncluded; // Some synonyms make it that the articles
                                      // appear several times. We combat this
                                      // by only allowing them to appear once.

    std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
    if ( ignoreDiacritics ) {
      wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
    }

    char buf[ 16384 ];

    for ( auto & x : chain ) {
      if ( articlesIncluded.find( x.articleOffset ) != articlesIncluded.end() ) {
        continue; // We already have this article in the body.
      }

      // Now load that article

      {
        QMutexLocker _( &indexFileMutex );
        indexFile.seek( x.articleOffset );

        if ( !indexFile.gets( buf, sizeof( buf ), true ) ) {
          throw exFailedToReadLineFromIndex();
        }
      }

      char * tab1 = strchr( buf, '\t' );

      if ( !tab1 ) {
        throw exMalformedIndexFileLine();
      }

      char * tab2 = strchr( tab1 + 1, '\t' );

      if ( !tab2 ) {
        throw exMalformedIndexFileLine();
      }

      // After tab1 should be article offset, after tab2 -- article size

      uint32_t articleOffset = decodeBase64( string( tab1 + 1, tab2 - tab1 - 1 ) );

      char * tab3 = strchr( tab2 + 1, '\t' );

      uint32_t articleSize;
      if ( tab3 ) {
        articleSize = decodeBase64( string( tab2 + 1, tab3 - tab2 - 1 ) );
      }
      else {
        articleSize = decodeBase64( tab2 + 1 );
      }

      string articleText;

      char * articleBody;
      {
        QMutexLocker _( &dzMutex );
        articleBody = dict_data_read_( dz, articleOffset, articleSize, 0, 0 );
      }

      if ( !articleBody ) {
        articleText = string( "<div class=\"dictd_article\">DICTZIP error: " ) + dict_error_str( dz ) + "</div>";
      }
      else {
        static QRegularExpression phonetic( R"(\\([^\\]+)\\)",
                                            QRegularExpression::CaseInsensitiveOption ); // phonetics: \stuff\ ...
        static QRegularExpression refs( R"(\{([^\{\}]+)\})",
                                        QRegularExpression::CaseInsensitiveOption ); // links: {stuff}
        static QRegularExpression links( "<a href=\"gdlookup://localhost/([^\"]*)\">",
                                         QRegularExpression::CaseInsensitiveOption );
        static QRegularExpression tags( "<[^>]*>", QRegularExpression::CaseInsensitiveOption );


        articleText = string( "<div class=\"dictd_article\"" );
        if ( isToLanguageRTL() ) {
          articleText += " dir=\"rtl\"";
        }
        articleText += ">";

        string convertedText = Html::preformat( articleBody, isToLanguageRTL() );
        free( articleBody );

        QString articleString = QString::fromUtf8( convertedText.c_str() )
                                  .replace( phonetic, R"(<span class="dictd_phonetic">\1</span>)" )
                                  .replace( refs, R"(<a href="gdlookup://localhost/\1">\1</a>)" );
        convertedText.erase();

        int pos = 0;

        QString articleNewString;
        QRegularExpressionMatchIterator it = links.globalMatch( articleString );
        while ( it.hasNext() ) {
          QRegularExpressionMatch match = it.next();
          articleNewString += articleString.mid( pos, match.capturedStart() - pos );
          pos = match.capturedEnd();

          QString link = match.captured( 1 );
          link.replace( tags, " " );
          link.replace( "&nbsp;", " " );

          QString newLink = match.captured();
          newLink.replace( 30,
                           match.capturedLength( 1 ),
                           QString::fromUtf8( QUrl::toPercentEncoding( link.simplified() ) ) );
          articleNewString += newLink;
        }
        if ( pos ) {
          articleNewString += articleString.mid( pos );
          articleString = articleNewString;
          articleNewString.clear();
        }


        articleString += "</div>";

        articleText += articleString.toUtf8().data();
      }

      // Ok. Now, does it go to main articles, or to alternate ones? We list
      // main ones first, and alternates after.

      // We do the case-folded comparison here.

      std::u32string headwordStripped = Folding::applySimpleCaseOnly( x.word );
      if ( ignoreDiacritics ) {
        headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
      }

      multimap< std::u32string, string > & mapToUse =
        ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

      mapToUse.insert( pair( Folding::applySimpleCaseOnly( x.word ), articleText ) );

      articlesIncluded.insert( x.articleOffset );
    }

    if ( mainArticles.empty() && alternateArticles.empty() ) {
      return std::make_shared< Dictionary::DataRequestInstant >( false );
    }

    string result;

    multimap< std::u32string, string >::const_iterator i;

    for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
      result += i->second;
    }

    for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
      result += i->second;
    }

    auto ret = std::make_shared< Dictionary::DataRequestInstant >( true );
    ret->appendString( result );

    return ret;
  }
  catch ( std::exception & e ) {
    return std::make_shared< Dictionary::DataRequestInstant >( QString( e.what() ) );
  }
}

QString const & DictdDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  sptr< Dictionary::DataRequest > req =
    getArticle( U"00databaseinfo", vector< std::u32string >(), std::u32string(), false );

  if ( req->dataSize() > 0 ) {
    dictionaryDescription = QString::fromUtf8( req->getFullData().data(), req->getFullData().size() );
  }
  else {
    dictionaryDescription = "NONE";
  }

  return dictionaryDescription;
}

void DictdDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) ) {
    FTS_index_completed.ref();
  }

  if ( haveFTSIndex() ) {
    return;
  }

  if ( ensureInitDone().size() ) {
    return;
  }


  qDebug( "DictD: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "DictD: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( QString::fromStdString( ftsIdxName ) );
  }
}

void DictdDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    char buf[ 16384 ];
    {
      QMutexLocker _( &indexFileMutex );
      indexFile.seek( articleAddress );

      if ( !indexFile.gets( buf, sizeof( buf ), true ) ) {
        throw exFailedToReadLineFromIndex();
      }
    }

    char * tab1 = strchr( buf, '\t' );

    if ( !tab1 ) {
      throw exMalformedIndexFileLine();
    }

    headword = QString::fromUtf8( buf, tab1 - buf );

    char * tab2 = strchr( tab1 + 1, '\t' );

    if ( !tab2 ) {
      throw exMalformedIndexFileLine();
    }

    // After tab1 should be article offset, after tab2 -- article size

    uint32_t articleOffset = decodeBase64( string( tab1 + 1, tab2 - tab1 - 1 ) );

    char * tab3 = strchr( tab2 + 1, '\t' );

    uint32_t articleSize;
    if ( tab3 ) {
      articleSize = decodeBase64( string( tab2 + 1, tab3 - tab2 - 1 ) );
    }
    else {
      articleSize = decodeBase64( tab2 + 1 );
    }

    string articleText;

    char * articleBody;
    {
      QMutexLocker _( &dzMutex );
      articleBody = dict_data_read_( dz, articleOffset, articleSize, 0, 0 );
    }

    if ( !articleBody ) {
      articleText = dict_error_str( dz );
    }
    else {
      static QRegularExpression phonetic( R"(\\([^\\]+)\\)",
                                          QRegularExpression::CaseInsensitiveOption ); // phonetics: \stuff\ ...
      static QRegularExpression refs( R"(\{([^\{\}]+)\})",
                                      QRegularExpression::CaseInsensitiveOption ); // links: {stuff}

      string convertedText = Html::preformat( articleBody, isToLanguageRTL() );
      free( articleBody );

      text = QString::fromUtf8( convertedText.data(), convertedText.size() )
               .replace( phonetic, R"(<span class="dictd_phonetic">\1</span>)" )
               .replace( refs, R"(<a href="gdlookup://localhost/\1">\1</a>)" );

      text = Html::unescape( text );
    }
  }
  catch ( std::exception & ex ) {
    qWarning( "DictD: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest >
DictdDictionary::getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

} // anonymous namespace

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    // Only allow .index suffixes

    if ( !Utils::endsWithIgnoreCase( fileName, ".index" ) ) {
      continue;
    }

    try {
      vector< string > dictFiles( 1, fileName );

      // Check if there is an 'abrv' file present
      string baseName( fileName, 0, fileName.size() - 5 );

      dictFiles.push_back( string() );

      if ( !File::tryPossibleName( baseName + "dict", dictFiles[ 1 ] )
           && !File::tryPossibleName( baseName + "dict.dz", dictFiles[ 1 ] ) ) {
        // No corresponding .dict file, skipping
        continue;
      }

      string dictId = Dictionary::makeDictionaryId( dictFiles );

      string indexFile = indicesDir + dictId;

      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        // Building the index
        string dictionaryName = nameFromFileName( dictFiles[ 0 ] );

        qDebug( "DictD: Building the index for dictionary: %s", dictionaryName.c_str() );

        initializing.indexingDictionary( dictionaryName );

        File::Index idx( indexFile, QIODevice::WriteOnly );

        IdxHeader idxHeader;

        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        IndexedWords indexedWords;

        File::Index indexFile( dictFiles[ 0 ], QIODevice::ReadOnly );

        // Read words from index until none's left.

        char buf[ 16384 ];

        do {
          uint32_t curOffset = indexFile.tell();

          if ( !indexFile.gets( buf, sizeof( buf ), true ) ) {
            break;
          }

          // Check that there are exactly two or three tabs in the record.
          char * tab1 = strchr( buf, '\t' );
          if ( tab1 ) {
            char * tab2 = strchr( tab1 + 1, '\t' );
            if ( tab2 ) {
              char * tab3 = strchr( tab2 + 1, '\t' );
              if ( tab3 ) {
                char * tab4 = strchr( tab3 + 1, '\t' );
                if ( tab4 ) {
                  qDebug( "Warning: too many tabs present, skipping: %s", buf );
                  continue;
                }

                // Handle the forth entry, if it exists. From dictfmt man:
                // When --index-keep-orig option is used fourth column is created
                // (if necessary) in .index file.
                indexedWords.addWord( Text::toUtf32( string( tab3 + 1, strlen( tab3 + 1 ) ) ), curOffset );
                ++idxHeader.wordCount;
              }
              indexedWords.addWord( Text::toUtf32( string( buf, strchr( buf, '\t' ) - buf ) ), curOffset );
              ++idxHeader.wordCount;
              ++idxHeader.articleCount;

              // Check for proper dictionary name
              if ( !strncmp( buf, "00databaseshort", 15 ) || !strncmp( buf, "00-database-short", 17 ) ) {
                // After tab1 should be article offset, after tab2 -- article size
                uint32_t articleOffset = decodeBase64( string( tab1 + 1, tab2 - tab1 - 1 ) );
                uint32_t articleSize   = decodeBase64( tab2 + 1 );

                DZ_ERRORS error;
                dictData * dz = dict_data_open( dictFiles[ 1 ].c_str(), &error, 0 );

                if ( dz ) {
                  char * articleBody = dict_data_read_( dz, articleOffset, articleSize, 0, 0 );
                  if ( articleBody ) {
                    char * eol;
                    if ( !strncmp( articleBody, "00databaseshort", 15 )
                         || !strncmp( articleBody, "00-database-short", 17 ) ) {
                      eol = strchr( articleBody, '\n' ); // skip the first line (headword itself)
                    }
                    else {
                      eol = articleBody; // No headword itself
                    }
                    if ( eol ) {
                      while ( *eol && Text::isspace( *eol ) ) {
                        ++eol; // skip spaces
                      }

                      // use only the single line for the dictionary title
                      char * endEol = strchr( eol, '\n' );
                      if ( endEol ) {
                        *endEol = 0;
                      }

                      qDebug( "DICT NAME: '%s'", eol );
                      dictionaryName = eol;
                    }
                  }
                  dict_data_close( dz );
                }
                else {
                  throw exDictzipError( string( dz_error_str( error ) ) + "(" + dictFiles[ 1 ] + ")" );
                }
              }
            }
            else {
              qDebug( "Warning: only a single tab present, skipping: %s", buf );
              continue;
            }
          }
          else {
            qDebug( "Warning: no tabs present, skipping: %s", buf );
            continue;
          }


        } while ( !indexFile.eof() );


        // Write dictionary name

        idx.write( (uint32_t)dictionaryName.size() );
        idx.write( dictionaryName.data(), dictionaryName.size() );

        // Build index

        IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

        idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
        idxHeader.indexRootOffset       = idxInfo.rootOffset;

        // That concludes it. Update the header.

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        // read languages from dictioanry file name
        auto langs = LangCoder::findLangIdPairFromPath( dictFiles[ 0 ] );

        // if no languages found, try dictionary's name
        if ( langs.first == 0 || langs.second == 0 ) {
          langs = LangCoder::findLangIdPairFromName( QString::fromStdString( dictionaryName ) );
        }

        idxHeader.langFrom = langs.first;
        idxHeader.langTo   = langs.second;

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }

      dictionaries.push_back( std::make_shared< DictdDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Dictd dictionary \"%s\" reading failed, error: %s", fileName.c_str(), e.what() );
    }
  }

  return dictionaries;
}

} // namespace DictdFiles
