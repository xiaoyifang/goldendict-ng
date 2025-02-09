/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <map>
#include <string>
#include <vector>

#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>
#include <QGuiApplication>

#include "config.hh"
#include "ex.hh"
#include "globalbroadcaster.hh"
#include "langcoder.hh"
#include "sptr.hh"
#include "utils.hh"
#include "text.hh"
#include <QtGlobal>

/// Abstract dictionary-related stuff
namespace Dictionary {

using std::vector;
using std::string;
using std::map;

DEF_EX( Ex, "Dictionary error", std::exception )
DEF_EX( exIndexOutOfRange, "The supplied index is out of range", Ex )
DEF_EX( exSliceOutOfRange, "The requested data slice is out of range", Ex )
DEF_EX( exRequestUnfinished, "The request hasn't yet finished", Ex )

DEF_EX_STR( exCantReadFile, "Can't read file", Dictionary::Ex )

/// When you request a search to be performed in a dictionary, you get
/// this structure in return. It accumulates search results over time.
/// The finished() signal is emitted when the search has finished and there's
/// no more matches to be expected. Note that before connecting to it, check
/// the result of isFinished() -- if it's 'true', the search was instantaneous.
/// Destroy the object when you are not interested in results anymore.
///
/// Creating, destroying and calling member functions of the requests is done
/// in the GUI thread, however. Therefore, it is important to make sure those
/// operations are fast (this is most important for word searches, where
/// new requests are created and old ones deleted immediately upon a user
/// changing query).
class Request: public QObject
{
  Q_OBJECT

public:
  Request( QObject * parent = nullptr ):
    QObject( parent )
  {
  }
  /// Returns whether the request has been processed in full and finished.
  /// This means that the data accumulated is final and won't change anymore.
  bool isFinished();

  /// Either returns an empty string in case there was no error processing
  /// the request, or otherwise a human-readable string describing the problem.
  /// Note that an empty result, such as a lack of word or of an article isn't
  /// an error -- but any kind of failure to connect to, or read the dictionary
  /// is.
  QString getErrorString();

  /// Cancels the ongoing request. This may make Request destruct faster some
  /// time in the future, Use this in preparation to destruct many Requests,
  /// so that they'd be cancelling in parallel. When the request was fully
  /// cancelled, it must emit the finished() signal, either as a result of an
  /// actual finish which has happened just before the cancellation, or solely as
  /// a result of a request being cancelled (in the latter case, the actual
  /// request result may be empty or incomplete). That is, finish() must be
  /// called by a derivative at least once if cancel() was called, either after
  /// or before it was called.
  virtual void cancel() = 0;

  virtual ~Request() {}

signals:

  /// This signal is emitted when more data becomes available. Local
  /// dictionaries typically don't call this, since it is preferred that all
  /// data would be available from them at once, but network dictionaries
  /// might call that.
  void updated();

  /// This signal is emitted when the request has been processed in full and
  /// finished. That is, it's emitted when isFinished() turns true.
  void finished();

  void matchCount( int );

protected:
  /// Called by derivatives to signal update().
  void update();

  /// Called by derivatives to set isFinished() flag and signal finished().
  void finish();

  /// Sets the error string to be returned by getErrorString().
  void setErrorString( QString const & );
  QWaitCondition cond;
  // Subclasses should be filling up the 'data' array, locking the mutex when
  // whey work with it.
  QMutex dataMutex;

private:

  QAtomicInt isFinishedFlag;

  QMutex errorStringMutex;
  QString errorString;
};

/// This structure represents the word found. In addition to holding the
/// word itself, it also holds its weight. It is 0 by default. Negative
/// values should be used to store distance from Levenstein-like matching
/// algorithms. Positive values are used by morphology matches.
struct WordMatch
{
  std::u32string word;
  int weight;

  WordMatch():
    weight( 0 )
  {
  }
  WordMatch( std::u32string const & word_ ):
    word( word_ ),
    weight( 0 )
  {
  }
  WordMatch( std::u32string const & word_, int weight_ ):
    word( word_ ),
    weight( weight_ )
  {
  }
};

/// This request type corresponds to all types of word searching operations.
class WordSearchRequest: public Request
{
  Q_OBJECT

public:

  WordSearchRequest():
    uncertain( false )
  {
  }

  /// Returns the number of matches found. The value can grow over time
  /// unless isFinished() is true.
  size_t matchesCount();

  /// Returns the match with the given zero-based index, which should be less
  /// than matchesCount().
  WordMatch operator[]( size_t index );

  /// Returns all the matches found. Since no further locking can or would be
  /// done, this can only be called after the request has finished.
  vector< WordMatch > & getAllMatches();

  /// Returns true if the match was uncertain -- that is, there may be more
  /// results in the dictionary itself, the dictionary index isn't good enough
  /// to tell that.
  bool isUncertain() const
  {
    return uncertain;
  }

  /// Add match if one is not presented in matches list
  void addMatch( WordMatch const & match );

protected:

  // Subclasses should be filling up the 'matches' array, locking the mutex when
  // whey work with it.
  QMutex dataMutex;

  vector< WordMatch > matches;
  bool uncertain;
};

/// This request type corresponds to any kinds of data responses where a
/// single large blob of binary data is returned. It currently used of article
/// bodies and resources.
class DataRequest: public Request
{
  Q_OBJECT

public:
  /// Returns the number of bytes read, with a -1 meaning that so far it's
  /// uncertain whether resource even exists or not, and any non-negative value
  /// meaning that that amount of bytes is not available.
  /// If -1 is still being returned after the request has finished, that means
  /// the resource wasn't found.
  long dataSize();

  void appendDataSlice( const void * buffer, size_t size );
  void appendString( std::string_view str );

  /// Writes "size" bytes starting from "offset" of the data read to the given
  /// buffer. "size + offset" must be <= than dataSize().
  void getDataSlice( size_t offset, size_t size, void * buffer );

  /// Returns all the data read. Since no further locking can or would be
  /// done, this can only be called after the request has finished.
  vector< char > & getFullData();

  DataRequest( QObject * parent = 0 ):
    Request( parent ),
    hasAnyData( false )
  {
  }
signals:
  void finishedArticle( QString articleText );

protected:
  bool hasAnyData; // With this being false, dataSize() always returns -1
  vector< char > data;
};

/// A helper class for synchronous word search implementations.
class WordSearchRequestInstant: public WordSearchRequest
{
  Q_OBJECT

public:

  WordSearchRequestInstant()
  {
    finish();
  }

  void cancel() override {}

  vector< WordMatch > & getMatches()
  {
    return matches;
  }

  void setUncertain( bool value )
  {
    uncertain = value;
  }
};

/// A helper class for synchronous data read implementations.
class DataRequestInstant: public DataRequest
{
public:

  DataRequestInstant( bool succeeded )
  {
    hasAnyData = succeeded;
    finish();
  }

  DataRequestInstant( QString const & errorString )
  {
    setErrorString( errorString );
    finish();
  }

  virtual void cancel() {}

  vector< char > & getData()
  {
    return data;
  }
};

/// Dictionary features. Different dictionaries can possess different features,
/// which hint at some of their aspects.
enum Feature {
  /// No features
  NoFeatures = 0,
  /// The dictionary is suitable to query when searching for compound expressions.
  SuitableForCompoundSearching = 1,
  WebSite                      = 2
};

Q_DECLARE_FLAGS( Features, Feature )
Q_DECLARE_OPERATORS_FOR_FLAGS( Features )

/// A dictionary. Can be used to query words.
class Class: public QObject
{
  Q_OBJECT

  string id;
  vector< string > dictionaryFiles;
  long indexedFtsDoc;

  long lastProgress = 0;

protected:
  QString dictionaryDescription;
  QIcon dictionaryIcon;
  bool dictionaryIconLoaded;
  bool can_FTS;
  QAtomicInt FTS_index_completed;
  bool synonymSearchEnabled;
  string dictionaryName;
  std::optional< bool > metadata_enable_fts = std::nullopt;
  // Load user icon if it exist
  // By default set icon to empty
  virtual void loadIcon() noexcept;

  static int getOptimalIconSize();

  /// Try load icon based on the main dict file name
  [[nodiscard]] bool loadIconFromFileName( QString const & mainDictFileName );
  /// Load an icon using a full image file path
  bool loadIconFromFilePath( QString const & filename );
  /// Generate icon based on a text
  bool loadIconFromText( const QString & iconUrl, QString const & text );

  static QString getAbbrName( QString const & text );
  static QColor intToFixedColor( int index );
  /// Make css content usable only for articles from this dictionary
  void isolateCSS( QString & css, QString const & wrapperSelector = QString() );

public:

  /// Creates a dictionary. The id should be made using
  /// Format::makeDictionaryId(), the dictionaryFiles is the file names the
  /// dictionary consists of.
  Class( string const & id, vector< string > const & dictionaryFiles );

  /// Called once after the dictionary is constructed. Usually called for each
  /// dictionaries once all dictionaries were made. The implementation should
  /// queue any initialization tasks the dictionary decided to postpone to
  /// threadpools, network requests etc, so the system could complete them
  /// in background.
  /// The default implementation does nothing.
  virtual void deferredInit();

  /// Returns the dictionary's id.
  string getId() noexcept
  {
    return id;
  }

  /// Returns the list of file names the dictionary consists of.
  vector< string > const & getDictionaryFilenames() noexcept
  {
    return dictionaryFiles;
  }

  /// Get the main folder that contains the dictionary, without the ending separator.
  /// If the dict don't have folder like website/program, an empty string will be returned.
  QString getContainingFolder() const;

  /// Returns the dictionary's full name, utf8.
  virtual string getName()
  {
    return dictionaryName;
  }

  virtual void setName( string _dictionaryName )
  {
    dictionaryName = _dictionaryName;
  }

  void setFtsEnable( bool _enable_FTS )
  {
    metadata_enable_fts = _enable_FTS;
  }

  /// Returns the features the dictionary possess. See the Feature enum for
  /// their list.
  virtual Features getFeatures() const noexcept
  {
    return NoFeatures;
  }

  /// Returns the number of articles in the dictionary.
  virtual unsigned long getArticleCount() noexcept = 0;

  void setIndexedFtsDoc( long _indexedFtsDoc )
  {
    indexedFtsDoc = _indexedFtsDoc;

    auto newProgress = getIndexingFtsProgress();
    if ( newProgress != lastProgress ) {
      lastProgress = newProgress;
      emit GlobalBroadcaster::instance()
        -> indexingDictionary( QString( "%1......%%2" ).arg( QString::fromStdString( getName() ) ).arg( newProgress ) );
    }
  }

  int getIndexingFtsProgress()
  {
    if ( haveFTSIndex() ) {
      return 100;
    }
    auto total = getArticleCount();
    if ( total == 0 )
      return 0;
    int progress = (int)indexedFtsDoc * 100 / total;
    return qMin( progress, 100 );
  }

  /// Returns the number of words in the dictionary. This can be equal to
  /// the number of articles, or can be larger if some synonyms are present.
  virtual unsigned long getWordCount() noexcept = 0;

  /// Returns the dictionary's icon.
  virtual QIcon const & getIcon() noexcept;

  /// Returns the dictionary's source language.
  virtual quint32 getLangFrom() const
  {
    return 0;
  }

  /// Returns the dictionary's target language.
  virtual quint32 getLangTo() const
  {
    return 0;
  }

  /// Looks up a given word in the dictionary, aiming for exact matches and
  /// prefix matches. If it's not possible to locate any prefix matches, no
  /// prefix results should be added. Not more than maxResults results should
  /// be stored. The whole operation is supposed to be fast, though some
  /// dictionaries, the network ones particularly, may of course be slow.
  virtual sptr< WordSearchRequest > prefixMatch( std::u32string const &, unsigned long maxResults ) = 0;

  /// Looks up a given word in the dictionary, aiming to find different forms
  /// of the given word by allowing suffix variations. This means allowing words
  /// which can be as short as the input word size minus maxSuffixVariation, or as
  /// long as the input word size plus maxSuffixVariation, which share at least
  /// the input word size minus maxSuffixVariation initial symbols.
  /// Since the goal is to find forms of the words, no matches where a word
  /// in the middle of a phrase got matched should be returned.
  /// The default implementation does nothing, returning an empty result.
  virtual sptr< WordSearchRequest >
  stemmedMatch( std::u32string const &, unsigned minLength, unsigned maxSuffixVariation, unsigned long maxResults );

  /// Finds known headwords for the given word, that is, the words for which
  /// the given word is a synonym. If a dictionary can't perform this operation,
  /// it should leave the default implementation which always returns an empty
  /// result.
  virtual sptr< WordSearchRequest > findHeadwordsForSynonym( std::u32string const & );

  /// For a given word, provides alternate writings of it which are to be looked
  /// up alongside with it. Transliteration dictionaries implement this. The
  /// default implementation returns an empty list. Note that this function is
  /// supposed to be very fast and simple, and the results are thus returned
  /// synchronously.
  virtual vector< std::u32string > getAlternateWritings( std::u32string const & ) noexcept;

  /// Returns a definition for the given word. The definition should
  /// be an html fragment (without html/head/body tags) in an utf8 encoding.
  /// The 'alts' vector could contain a list of words the definitions of which
  /// should be included in the output as well, being treated as additional
  /// synonyms for the main word.
  /// context is a dictionary-specific data, currently only used for the
  /// 'Websites' feature.
  virtual sptr< DataRequest > getArticle( std::u32string const &,
                                          vector< std::u32string > const & alts,
                                          std::u32string const & context = std::u32string(),
                                          bool ignoreDiacritics          = false ) = 0;

  /// Loads contents of a resource named 'name' into the 'data' vector. This is
  /// usually a picture file referenced in the article or something like that.
  /// The default implementation always returns the non-existing resource
  /// response.
  virtual sptr< DataRequest > getResource( string const & /*name*/ );

  /// Returns a results of full-text search of given string similar getArticle().
  virtual sptr< DataRequest >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics );

  // Return dictionary description if presented
  virtual QString const & getDescription();

  // Return dictionary main file name
  virtual QString getMainFilename();

  /// Check text direction
  bool isFromLanguageRTL()
  {
    return LangCoder::isLanguageRTL( getLangFrom() );
  }
  bool isToLanguageRTL()
  {
    return LangCoder::isLanguageRTL( getLangTo() );
  }

  /// Return true if dictionary is local dictionary
  virtual bool isLocalDictionary()
  {
    return false;
  }

  /// Dictionary can full-text search
  bool canFTS()
  {
    return can_FTS;
  }

  /// Dictionary have index for full-text search
  bool haveFTSIndex()
  {
    return Utils::AtomicInt::loadAcquire( FTS_index_completed ) != 0;
  }

  /// Make index for full-text search
  virtual void makeFTSIndex( QAtomicInt & ) {}

  /// Set full-text search parameters
  virtual void setFTSParameters( Config::FullTextSearch const & ) {}

  /// Retrieve all dictionary headwords
  virtual bool getHeadwords( QStringList & )
  {
    return false;
  }
  virtual void findHeadWordsWithLenth( int &, QSet< QString > * /*headwords*/, uint32_t ) {}

  /// Enable/disable search via synonyms
  void setSynonymSearchEnabled( bool enabled )
  {
    synonymSearchEnabled = enabled;
  }

  virtual ~Class() = default;
};

/// Callbacks to be used when the dictionaries are being initialized.
class Initializing
{
public:

  /// Called by the Format instance to notify the caller that the given
  /// dictionary is being indexed. Since indexing can take some time, this
  /// is useful to show in some kind of a splash screen.
  /// The dictionaryName is in utf8.
  virtual void indexingDictionary( string const & dictionaryName ) noexcept = 0;
  virtual void loadingDictionary( string const & dictionaryName ) noexcept  = 0;

  virtual ~Initializing() = default;
};

/// Generates an id based on the set of file names which the dictionary
/// consists of. The resulting id is an alphanumeric hex value made by
/// hashing the file names. This id should be used to identify dictionary
/// and for the index file name, if one is needed.
/// This function is supposed to be used by dictionary implementations.
string makeDictionaryId( vector< string > const & dictionaryFiles ) noexcept;

/// Checks if it is needed to regenerate index file based on its timestamp
/// and the timestamps of the dictionary files. If some files are newer than
/// the index file, or the index file doesn't exist, returns true. If some
/// dictionary files don't exist, returns true, too.
/// This function is supposed to be used by dictionary implementations.
bool needToRebuildIndex( vector< string > const & dictionaryFiles, string const & indexFile ) noexcept;

string getFtsSuffix();
/// Returns a random dictionary id useful for interactively created
/// dictionaries.
QString generateRandomDictionaryId();

QMap< std::string, sptr< Dictionary::Class > > dictToMap( std::vector< sptr< Dictionary::Class > > const & dicts );

} // namespace Dictionary
