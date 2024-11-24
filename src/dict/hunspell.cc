/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "hunspell.hh"
#include "text.hh"
#include "htmlescape.hh"
#include "iconv.hh"
#include "folding.hh"
#include "language.hh"
#include "langcoder.hh"
#include <QRegularExpression>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <set>
#include "utils.hh"
#include <QtConcurrentRun>
#include <hunspell/hunspell.hxx>

namespace HunspellMorpho {

using namespace Dictionary;


namespace {

class HunspellDictionary: public Dictionary::Class
{
  Hunspell hunspell;

#ifdef Q_OS_WIN32
  static string Utf8ToLocal8Bit( string const & name )
  {
    return string( QString::fromUtf8( name.c_str() ).toLocal8Bit().data() );
  }
#endif

public:

  /// files[ 0 ] should be .aff file, files[ 1 ] should be .dic file.
  HunspellDictionary( string const & id, string const & name_, vector< string > const & files ):
    Dictionary::Class( id, files ),
#ifdef Q_OS_WIN32
    hunspell( Utf8ToLocal8Bit( files[ 0 ] ).c_str(), Utf8ToLocal8Bit( files[ 1 ] ).c_str() )
#else
    hunspell( files[ 0 ].c_str(), files[ 1 ].c_str() )
#endif
  {
    dictionaryName = name_;
  }

  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( std::u32string const &, unsigned long maxResults ) override;

  sptr< WordSearchRequest > findHeadwordsForSynonym( std::u32string const & ) override;

  sptr< DataRequest >
  getArticle( std::u32string const &, vector< std::u32string > const & alts, std::u32string const &, bool ) override;

  bool isLocalDictionary() override
  {
    return true;
  }

  vector< std::u32string > getAlternateWritings( const std::u32string & word ) noexcept override;

protected:

  void loadIcon() noexcept override;

private:

  // We used to have a separate mutex for each Hunspell instance, assuming
  // that its code was reentrant (though probably not thread-safe). However,
  // crashes were discovered later when using several Hunspell dictionaries
  // simultaneously, and we've switched to have a single mutex for all hunspell
  // calls - evidently it's not really reentrant.
  static QMutex & getHunspellMutex()
  {
    static QMutex mutex;
    return mutex;
  }
  //  QMutex hunspellMutex;
};

/// Decodes the given string returned by the hunspell object. May throw
/// Iconv::Ex
std::u32string decodeFromHunspell( Hunspell &, char const * );

/// Generates suggestions via hunspell
QList< std::u32string > suggest( std::u32string & word, QMutex & hunspellMutex, Hunspell & hunspell );

/// Generates suggestions for compound expression
void getSuggestionsForExpression( std::u32string const & expression,
                                  vector< std::u32string > & suggestions,
                                  QMutex & hunspellMutex,
                                  Hunspell & hunspell );

/// Returns true if the string contains whitespace, false otherwise
bool containsWhitespace( std::u32string const & str )
{
  char32_t const * next = str.c_str();

  for ( ; *next; ++next ) {
    if ( Folding::isWhitespace( *next ) ) {
      return true;
    }
  }

  return false;
}

void HunspellDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  // Remove the extension
  fileName.chop( 3 );

  if ( !loadIconFromFile( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_hunspell.png" );
  }

  dictionaryIconLoaded = true;
}

vector< std::u32string > HunspellDictionary::getAlternateWritings( std::u32string const & word ) noexcept
{
  vector< std::u32string > results;

  if ( containsWhitespace( word ) ) {
    getSuggestionsForExpression( word, results, getHunspellMutex(), hunspell );
  }

  return results;
}

/// HunspellDictionary::getArticle()

class HunspellArticleRequest: public Dictionary::DataRequest
{

  QMutex & hunspellMutex;
  Hunspell & hunspell;
  std::u32string word;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  HunspellArticleRequest( std::u32string const & word_, QMutex & hunspellMutex_, Hunspell & hunspell_ ):
    hunspellMutex( hunspellMutex_ ),
    hunspell( hunspell_ ),
    word( word_ )
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

  ~HunspellArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void HunspellArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  vector< string > suggestions;

  try {
    std::u32string trimmedWord = Folding::trimWhitespaceOrPunct( word );

    if ( containsWhitespace( trimmedWord ) ) {
      // For now we don't analyze whitespace-containing phrases
      finish();
      return;
    }

    QMutexLocker _( &hunspellMutex );

    string trimmedWord_utf8 = Iconv::toUtf8( Iconv::GdWchar, trimmedWord.data(), trimmedWord.size() );

    if ( hunspell.spell( trimmedWord_utf8 ) ) {
      // Good word -- no spelling suggestions then.
      finish();
      return;
    }

    suggestions = hunspell.suggest( trimmedWord_utf8 );
    if ( !suggestions.empty() ) {
      // There were some suggestions made for us. Make an appropriate output.

      string result = "<div class=\"gdspellsuggestion\">"
        + Html::escape( QCoreApplication::translate( "Hunspell", "Spelling suggestions: " ).toUtf8().data() );

      std::u32string lowercasedWord = Folding::applySimpleCaseOnly( word );

      for ( vector< string >::size_type x = 0; x < suggestions.size(); ++x ) {
        std::u32string suggestion = decodeFromHunspell( hunspell, suggestions[ x ].c_str() );

        if ( Folding::applySimpleCaseOnly( suggestion ) == lowercasedWord ) {
          // If among suggestions we see the same word just with the different
          // case, we botch the search -- our searches are case-insensitive, and
          // there's no need for suggestions on a good word.

          finish();

          return;
        }
        string suggestionUtf8 = Text::toUtf8( suggestion );

        result += "<a href=\"bword:";
        result += Html::escape( suggestionUtf8 ) + "\">";
        result += Html::escape( suggestionUtf8 ) + "</a>";

        if ( x != suggestions.size() - 1 ) {
          result += ", ";
        }
      }

      result += "</div>";

      appendString( result );

      hasAnyData = true;
    }
  }
  catch ( Iconv::Ex & e ) {
    qWarning( "Hunspell: charset conversion error, no processing's done: %s", e.what() );
  }
  catch ( std::exception & e ) {
    qWarning( "Hunspell: error: %s", e.what() );
  }

  finish();
}

sptr< DataRequest > HunspellDictionary::getArticle( std::u32string const & word,
                                                    vector< std::u32string > const &,
                                                    std::u32string const &,
                                                    bool )

{
  return std::make_shared< HunspellArticleRequest >( word, getHunspellMutex(), hunspell );
}

/// HunspellDictionary::findHeadwordsForSynonym()

class HunspellHeadwordsRequest: public Dictionary::WordSearchRequest
{

  QMutex & hunspellMutex;
  Hunspell & hunspell;
  std::u32string word;

  QAtomicInt isCancelled;
  QFuture< void > f;


public:

  HunspellHeadwordsRequest( std::u32string const & word_, QMutex & hunspellMutex_, Hunspell & hunspell_ ):
    hunspellMutex( hunspellMutex_ ),
    hunspell( hunspell_ ),
    word( word_ )
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

  ~HunspellHeadwordsRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void HunspellHeadwordsRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  std::u32string trimmedWord = Folding::trimWhitespaceOrPunct( word );

  if ( trimmedWord.size() > 80 ) {
    // We won't do anything for overly long sentences since that would probably
    // only waste time.
    finish();
    return;
  }

  if ( containsWhitespace( trimmedWord ) ) {
    vector< std::u32string > results;

    getSuggestionsForExpression( trimmedWord, results, hunspellMutex, hunspell );

    QMutexLocker _( &dataMutex );
    for ( const auto & result : results ) {
      matches.push_back( result );
    }
  }
  else {
    QList< std::u32string > suggestions = suggest( trimmedWord, hunspellMutex, hunspell );

    if ( !suggestions.empty() ) {
      QMutexLocker _( &dataMutex );

      for ( const auto & suggestion : suggestions ) {
        matches.push_back( suggestion );
      }
    }
  }

  finish();
}

QList< std::u32string > suggest( std::u32string & word, QMutex & hunspellMutex, Hunspell & hunspell )
{
  QList< std::u32string > result;

  try {
    QMutexLocker _( &hunspellMutex );

    auto suggestions = hunspell.analyze( Iconv::toUtf8( Iconv::GdWchar, word.data(), word.size() ) );
    if ( !suggestions.empty() ) {
      // There were some suggestions made for us. Make an appropriate output.

      std::u32string lowercasedWord = Folding::applySimpleCaseOnly( word );

      static QRegularExpression cutStem( R"(^\s*st:(((\s+(?!\w{2}:)(?!-)(?!\+))|\S+)+))" );

      for ( const auto & x : suggestions ) {
        QString suggestion = QString::fromStdU32String( decodeFromHunspell( hunspell, x.c_str() ) );

        // Strip comments
        int n = suggestion.indexOf( '#' );
        if ( n >= 0 ) {
          suggestion.chop( suggestion.length() - n );
        }

        qDebug( ">>>Sugg: %s", suggestion.toLocal8Bit().data() );

        auto match = cutStem.match( suggestion.trimmed() );
        if ( match.hasMatch() ) {
          std::u32string alt = match.captured( 1 ).toStdU32String();

          if ( Folding::applySimpleCaseOnly( alt ) != lowercasedWord ) // No point in providing same word
          {
            result.append( alt );
          }
        }
      }
    }
  }
  catch ( Iconv::Ex & e ) {
    qWarning( "Hunspell: charset conversion error, no processing's done: %s", e.what() );
  }

  return result;
}


sptr< WordSearchRequest > HunspellDictionary::findHeadwordsForSynonym( std::u32string const & word )

{
  return std::make_shared< HunspellHeadwordsRequest >( word, getHunspellMutex(), hunspell );
}


/// HunspellDictionary::prefixMatch()

class HunspellPrefixMatchRequest: public Dictionary::WordSearchRequest
{

  QMutex & hunspellMutex;
  Hunspell & hunspell;
  std::u32string word;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  HunspellPrefixMatchRequest( std::u32string const & word_, QMutex & hunspellMutex_, Hunspell & hunspell_ ):
    hunspellMutex( hunspellMutex_ ),
    hunspell( hunspell_ ),
    word( word_ )
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

  ~HunspellPrefixMatchRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void HunspellPrefixMatchRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  try {
    std::u32string trimmedWord = Folding::trimWhitespaceOrPunct( word );

    if ( trimmedWord.empty() || containsWhitespace( trimmedWord ) ) {
      // For now we don't analyze whitespace-containing phrases
      finish();
      return;
    }

    QMutexLocker _( &hunspellMutex );

    if ( hunspell.spell( Iconv::toUtf8( Iconv::GdWchar, trimmedWord.data(), trimmedWord.size() ) ) ) {
      // Known word -- add it to the result

      QMutexLocker _( &dataMutex );

      matches.push_back( WordMatch( trimmedWord, 1 ) );
    }
  }
  catch ( Iconv::Ex & e ) {
    qWarning( "Hunspell: charset conversion error, no processing's done: %s", e.what() );
  }

  finish();
}

sptr< WordSearchRequest > HunspellDictionary::prefixMatch( std::u32string const & word, unsigned long /*maxResults*/ )

{
  return std::make_shared< HunspellPrefixMatchRequest >( word, getHunspellMutex(), hunspell );
}

void getSuggestionsForExpression( std::u32string const & expression,
                                  vector< std::u32string > & suggestions,
                                  QMutex & hunspellMutex,
                                  Hunspell & hunspell )
{
  // Analyze each word separately and use the first two suggestions, if any.
  // This is useful for compound expressions where some words is
  // in different form, e.g. "dozing off" -> "doze off".

  std::u32string trimmedWord = Folding::trimWhitespaceOrPunct( expression );
  std::u32string word, punct;
  QList< std::u32string > words;

  suggestions.clear();

  // Parse string to separate words

  for ( char32_t const * c = trimmedWord.c_str();; ++c ) {
    if ( !*c || Folding::isPunct( *c ) || Folding::isWhitespace( *c ) ) {
      if ( word.size() ) {
        words.push_back( word );
        word.clear();
      }
      if ( *c ) {
        punct.push_back( *c );
      }
    }
    else {
      if ( punct.size() ) {
        words.push_back( punct );
        punct.clear();
      }
      if ( *c ) {
        word.push_back( *c );
      }
    }
    if ( !*c ) {
      break;
    }
  }

  if ( words.size() > 21 ) {
    // Too many words - no suggestions
    return;
  }

  // Combine result strings from suggestions

  QList< std::u32string > results;

  for ( const auto & i : words ) {
    word = i;
    if ( Folding::isPunct( word[ 0 ] ) || Folding::isWhitespace( word[ 0 ] ) ) {
      for ( auto & result : results ) {
        result.append( word );
      }
    }
    else {
      QList< std::u32string > sugg = suggest( word, hunspellMutex, hunspell );
      int suggNum           = sugg.size() + 1;
      if ( suggNum > 3 ) {
        suggNum = 3;
      }
      int resNum = results.size();
      std::u32string resultStr;

      if ( resNum == 0 ) {
        for ( int k = 0; k < suggNum; k++ ) {
          results.push_back( k == 0 ? word : sugg.at( k - 1 ) );
        }
      }
      else {
        for ( int j = 0; j < resNum; j++ ) {
          resultStr = results.at( j );
          for ( int k = 0; k < suggNum; k++ ) {
            if ( k == 0 ) {
              results[ j ].append( word );
            }
            else {
              results.push_back( resultStr + sugg.at( k - 1 ) );
            }
          }
        }
      }
    }
  }

  for ( const auto & result : results ) {
    if ( result != trimmedWord ) {
      suggestions.push_back( result );
    }
  }
}

std::u32string decodeFromHunspell( Hunspell & hunspell, char const * str )
{
  Iconv conv( hunspell.get_dic_encoding() );

  void const * in = str;
  size_t inLeft   = strlen( str );

  vector< char32_t > result( inLeft + 1 ); // +1 isn't needed, but see above

  void * out     = &result.front();
  size_t outLeft = result.size() * sizeof( char32_t );

  QString convStr = conv.convert( in, inLeft );
  return convStr.toStdU32String();
}
} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( Config::Hunspell const & cfg )

{
  vector< sptr< Dictionary::Class > > result;

  vector< DataFiles > dataFiles = findDataFiles( cfg.dictionariesPath );


  for ( const auto & enabledDictionarie : cfg.enabledDictionaries ) {
    for ( unsigned d = dataFiles.size(); d--; ) {
      if ( dataFiles[ d ].dictId == enabledDictionarie ) {
        // Found it

        vector< string > dictFiles;

        dictFiles.push_back( QDir::toNativeSeparators( dataFiles[ d ].affFileName ).toStdString() );
        dictFiles.push_back( QDir::toNativeSeparators( dataFiles[ d ].dicFileName ).toStdString() );

        result.push_back( std::make_shared< HunspellDictionary >( Dictionary::makeDictionaryId( dictFiles ),
                                                                  dataFiles[ d ].dictName.toUtf8().data(),
                                                                  dictFiles ) );
        break;
      }
    }
  }

  return result;
}

vector< DataFiles > findDataFiles( QString const & path )
{
  // Empty path means unconfigured directory
  if ( path.isEmpty() ) {
    return vector< DataFiles >();
  }

  QDir dir( path );

  // Find all affix files

  QFileInfoList affixFiles = dir.entryInfoList( ( QStringList() << "*.aff"
                                                                << "*.AFF" ),
                                                QDir::Files );

  vector< DataFiles > result;
  std::set< QString > presentNames;

  for ( QFileInfoList::const_iterator i = affixFiles.constBegin(); i != affixFiles.constEnd(); ++i ) {
    QString affFileName = i->absoluteFilePath();

    // See if there's a corresponding .dic file
    QString dicFileNameBase = affFileName.mid( 0, affFileName.size() - 3 );

    QString dicFileName = dicFileNameBase + "dic";

    if ( !QFile( dicFileName ).exists() ) {
      dicFileName = dicFileNameBase + "DIC";
      if ( !QFile( dicFileName ).exists() ) {
        continue; // No dic file
      }
    }

    QString dictId = i->fileName();
    dictId.chop( 4 );

    QString dictBaseId =
      dictId.size() < 3 ? dictId : ( ( dictId[ 2 ] == '-' || dictId[ 2 ] == '_' ) ? dictId.mid( 0, 2 ) : QString() );

    dictBaseId = dictBaseId.toLower();

    // Try making up good readable name from dictBaseId

    QString localizedName;

    if ( dictBaseId.size() == 2 ) {
      localizedName = Language::localizedNameForId( LangCoder::code2toInt( dictBaseId.toLatin1().data() ) );
    }

    QString dictName = dictId;

    if ( localizedName.size() ) {
      dictName = localizedName;

      if ( dictId.size() > 2 && ( dictId[ 2 ] == '-' || dictId[ 2 ] == '_' )
           && dictId.mid( 3 ).toLower() != dictBaseId ) {
        dictName += " (" + dictId.mid( 3 ) + ")";
      }
    }

    dictName = QCoreApplication::translate( "Hunspell", "%1 Morphology" ).arg( dictName );

    if ( presentNames.insert( dictName ).second ) {
      // Only include dictionaries with unique names. This combats stuff
      // like symlinks en-US->en_US and such

      result.push_back( DataFiles( affFileName, dicFileName, dictId, dictName ) );
    }
  }

  return result;
}

} // namespace HunspellMorpho
