/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef __DICTIONARY_HH_INCLUDED__
#define __DICTIONARY_HH_INCLUDED__

#include <map>
#include <string>
#include <vector>

#include <QMutex>
#include <QObject>
#include <QString>
#include <QWaitCondition>

#include "config.hh"
#include "ex.hh"
#include "globalbroadcaster.hh"
#include "langcoder.hh"
#include "sptr.hh"
#include "utils.hh"
#include "wstring.hh"
#include "dictionary_requests.hh"

/// Abstract dictionary-related stuff
namespace Dictionary {

using std::vector;
using std::string;
using gd::wstring;
using std::map;

enum Property
{
  Author,
  Copyright,
  Description,
  Email
};

DEF_EX( Ex, "Dictionary error", std::exception )
DEF_EX_STR( exCantReadFile, "Can't read file", Dictionary::Ex )

/// Dictionary features. Different dictionaries can possess different features,
/// which hint at some of their aspects.
enum Feature
{
  /// No features
  NoFeatures = 0,
  /// The dictionary is suitable to query when searching for compound expressions.
  SuitableForCompoundSearching = 1
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

  // Load user icon if it exist
  // By default set icon to empty
  virtual void loadIcon() noexcept;

  // Load icon from filename directly if isFullName == true
  // else treat filename as name without extension
  bool loadIconFromFile( QString const & filename, bool isFullName = false );
  bool loadIconFromText( QString iconUrl, QString const & text );

  QString getAbbrName( QString const & text );

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
  { return id; }

  /// Returns the list of file names the dictionary consists of.
  vector< string > const & getDictionaryFilenames() noexcept
  { return dictionaryFiles; }

  /// Get the main folder that contains the dictionary, without the ending separator .
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

  /// Returns all the available properties, like the author's name, copyright,
  /// description etc. All strings are in utf8.
  virtual map< Property, string > getProperties() noexcept=0;

  /// Returns the features the dictionary possess. See the Feature enum for
  /// their list.
  virtual Features getFeatures() const noexcept
  { return NoFeatures; }

  /// Returns the number of articles in the dictionary.
  virtual unsigned long getArticleCount() noexcept=0;

  void setIndexedFtsDoc(long _indexedFtsDoc)
  {
    indexedFtsDoc = _indexedFtsDoc;

    auto newProgress = getIndexingFtsProgress();
    if ( newProgress != lastProgress ) {
      lastProgress = newProgress;
      emit GlobalBroadcaster::instance()->indexingDictionary(
        QString( "%1......%%2" ).arg( QString::fromStdString( getName() ) ).arg( newProgress ) );
    }
  }

  int getIndexingFtsProgress(){
    auto total = getArticleCount();
    if(total==0)
      return 0 ;
    return indexedFtsDoc*100/total;
  }

  /// Returns the number of words in the dictionary. This can be equal to
  /// the number of articles, or can be larger if some synonyms are present.
  virtual unsigned long getWordCount() noexcept=0;

  /// Returns the dictionary's icon.
  virtual QIcon const & getIcon() noexcept;

  /// Returns the dictionary's source language.
  virtual quint32 getLangFrom() const
  { return 0; }

  /// Returns the dictionary's target language.
  virtual quint32 getLangTo() const
  { return 0; }

  /// Looks up a given word in the dictionary, aiming for exact matches and
  /// prefix matches. If it's not possible to locate any prefix matches, no
  /// prefix results should be added. Not more than maxResults results should
  /// be stored. The whole operation is supposed to be fast, though some
  /// dictionaries, the network ones particularly, may of course be slow.
  virtual sptr< Request::WordSearch > prefixMatch( wstring const &,
                                                 unsigned long maxResults ) =0;

  /// Looks up a given word in the dictionary, aiming to find different forms
  /// of the given word by allowing suffix variations. This means allowing words
  /// which can be as short as the input word size minus maxSuffixVariation, or as
  /// long as the input word size plus maxSuffixVariation, which share at least
  /// the input word size minus maxSuffixVariation initial symbols.
  /// Since the goal is to find forms of the words, no matches where a word
  /// in the middle of a phrase got matched should be returned.
  /// The default implementation does nothing, returning an empty result.
  virtual sptr< Request::WordSearch > stemmedMatch( wstring const &,
                                                  unsigned minLength,
                                                  unsigned maxSuffixVariation,
                                                  unsigned long maxResults ) ;

  /// Finds known headwords for the given word, that is, the words for which
  /// the given word is a synonym. If a dictionary can't perform this operation,
  /// it should leave the default implementation which always returns an empty
  /// result.
  virtual sptr< Request::WordSearch > findHeadwordsForSynonym( wstring const & )
    ;

  /// For a given word, provides alternate writings of it which are to be looked
  /// up alongside with it. Transliteration dictionaries implement this. The
  /// default implementation returns an empty list. Note that this function is
  /// supposed to be very fast and simple, and the results are thus returned
  /// synchronously.
  virtual vector< wstring > getAlternateWritings( wstring const & )
    noexcept;

  /// Returns a definition for the given word. The definition should
  /// be an html fragment (without html/head/body tags) in an utf8 encoding.
  /// The 'alts' vector could contain a list of words the definitions of which
  /// should be included in the output as well, being treated as additional
  /// synonyms for the main word.
  /// context is a dictionary-specific data, currently only used for the
  /// 'Websites' feature.
  virtual sptr< Request::Article > getArticle( wstring const &,
                                          vector< wstring > const & alts,
                                          wstring const & context = wstring(),
                                          bool ignoreDiacritics = false )
    =0;

  /// Loads contents of a resource named 'name' into the 'data' vector. This is
  /// usually a picture file referenced in the article or something like that.
  /// The default implementation always returns the non-existing resource
  /// response.
  virtual sptr< Request::Blob > getResource( string const & /*name*/ )
    ;

  /// Returns a results of full-text search of given string similar getArticle().
  virtual sptr< Request::Blob >
  getSearchResults( QString const & searchString, int searchMode, bool matchCase, bool ignoreDiacritics );

  // Return dictionary description if presented
  virtual QString const& getDescription();

  // Return dictionary main file name
  virtual QString getMainFilename();

  /// Check text direction
  bool isFromLanguageRTL()
  { return LangCoder::isLanguageRTL( getLangFrom() ); }
  bool isToLanguageRTL()
  { return LangCoder::isLanguageRTL( getLangTo() ); }

  /// Return true if dictionary is local dictionary
  virtual bool isLocalDictionary()
  { return false; }

  /// Dictionary can full-text search
  bool canFTS()
  { return can_FTS; }

  /// Dictionary have index for full-text search
  bool haveFTSIndex()
  { return Utils::AtomicInt::loadAcquire( FTS_index_completed ) != 0; }

  /// Make index for full-text search
  virtual void makeFTSIndex( QAtomicInt &, bool )
  {}

  /// Set full-text search parameters
  virtual void setFTSParameters( Config::FullTextSearch const & )
  {}

  /// Retrieve all dictionary headwords
  virtual bool getHeadwords( QStringList & )
  { return false; }
  virtual  void findHeadWordsWithLenth( int &, QSet< QString > * /*headwords*/, uint32_t ){}

  /// Enable/disable search via synonyms
  void setSynonymSearchEnabled( bool enabled )
  { synonymSearchEnabled = enabled; }

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
  virtual void indexingDictionary( string const & dictionaryName ) noexcept=0;

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
bool needToRebuildIndex( vector< string > const & dictionaryFiles,
                         string const & indexFile ) noexcept;

string getFtsSuffix();
/// Returns a random dictionary id useful for interactively created
/// dictionaries.
QString generateRandomDictionaryId();

QMap< std::string, sptr< Dictionary::Class > >
dictToMap( std::vector< sptr< Dictionary::Class > > const & dicts );

}

#endif

