/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dict/dictionary.hh"
#include "dict/utils/dictfile.hh"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>
#include <QFuture>
#include <QList>
#include <lmdb.h>
#include <QFileInfo>
#include <type_traits>


/// A base for the dictionary which creates a btree index to look up
/// the words.
namespace BtreeIndexing {

using std::string;
using std::vector;
using std::map;

enum {
  /// This is to be bumped up each time the internal format changes.
  /// The value isn't used here by itself, it is supposed to be added
  /// to each dictionary's internal format version.
  FormatVersion = 5,
  //the indexedzip parse logic version
  ZipParseLogicVersion = 1
};

// These exceptions which might be thrown during the index traversal

DEF_EX( exIndexWasNotOpened, "The index wasn't opened", Dictionary::Ex )
DEF_EX( exFailedToDecompressNode, "Failed to decompress a btree's node", Dictionary::Ex )
DEF_EX( exCorruptedChainData, "Corrupted chain data in the leaf of a btree encountered", Dictionary::Ex )

/// Checks whether the index file's header is old or corrupted.
/// Returns true if the index needs to be rebuilt.
/// This reads the header from the index file and validates:
///   1. The file can be read and header size matches
///   2. The signature matches the expected value
///   3. The formatVersion matches the expected value
///   4. The sourceLastModified timestamp (at the end of the header) matches
///      the current dictionary files' maximum modification time
///   5. Any additional validation via the extraCheck callback
///
/// Template parameters:
///   IdxHeader - the dictionary-specific header struct (must start with signature and formatVersion)
///   ExtraCheckFn - optional callable for additional version checks (e.g. parserVersion, foldingVersion)
///
/// The IdxHeader struct's LAST field must be `uint64_t sourceLastModified`.
template< typename IdxHeader, typename ExtraCheckFn = void * >
bool indexIsOldOrBad( const string & indexFile,
                      const vector< string > & dictFiles,
                      uint32_t expectedSignature,
                      uint32_t expectedFormatVersion,
                      ExtraCheckFn * extraCheck = nullptr )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );
  IdxHeader header;

  if ( idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != expectedSignature
       || header.formatVersion != expectedFormatVersion ) {
    return true;
  }

  // Run any extra validation (e.g. parserVersion, foldingVersion, hasZipFile, etc.)
  if constexpr ( !std::is_same_v< ExtraCheckFn, void * > ) {
    if ( extraCheck && !( *extraCheck )( header ) ) {
      return true;
    }
  }

  // Check if any of the dictionary files were modified after the index was built
  qint64 lastModified = 0;
  for ( const auto & dictionaryFile : dictFiles ) {
    QFileInfo fileInfo( QString::fromUtf8( dictionaryFile.c_str() ) );
    if ( fileInfo.exists() ) {
      qint64 ts = fileInfo.lastModified().toMSecsSinceEpoch();
      if ( ts > lastModified ) {
        lastModified = ts;
      }
    }
  }

  return header.sourceLastModified != (uint64_t)lastModified;
}

/// Computes the maximum lastModified timestamp from the given dictionary files.
/// This should be called when building the index and the result stored in IdxHeader::sourceLastModified.
inline uint64_t computeSourceLastModified( const vector< string > & dictFiles )
{
  qint64 lastModified = 0;
  for ( const auto & dictionaryFile : dictFiles ) {
    QFileInfo fileInfo( QString::fromUtf8( dictionaryFile.c_str() ) );
    if ( fileInfo.exists() ) {
      qint64 ts = fileInfo.lastModified().toMSecsSinceEpoch();
      if ( ts > lastModified ) {
        lastModified = ts;
      }
    }
  }
  return (uint64_t)lastModified;
}

/// This structure describes a word linked to its translation. The
/// translation is represented as an abstract 32-bit offset.
struct WordArticleLink
{
  string word, prefix; // in utf8
  uint32_t articleOffset = 0;

  WordArticleLink() = default;

  WordArticleLink( const string & word_, uint32_t articleOffset_, const string & prefix_ = string() ):
    word( word_ ),
    prefix( prefix_ ),
    articleOffset( articleOffset_ )
  {
  }
};

/// Information needed to open the index
struct IndexInfo
{
  uint32_t btreeMaxElements, rootOffset;
  string suffix;

  IndexInfo( uint32_t btreeMaxElements_, uint32_t rootOffset_, const string & suffix_ = "" ):
    btreeMaxElements( btreeMaxElements_ ),
    rootOffset( rootOffset_ ),
    suffix( suffix_ )
  {
  }
};

/// Base btree indexing class which allows using what buildIndex() function
/// created. It's quite low-lovel and is basically a set of 'building blocks'
/// functions.
class BtreeIndex
{
public:

  BtreeIndex();

  /// Opens the index. The file reference is saved to be used for
  /// subsequent lookups.
  /// The mutex is the one to be locked when working with the file.
  void openIndex( const IndexInfo &, File::Index &, QMutex & );

  /// Finds articles that match the given string. A case-insensitive search
  /// is performed.
  vector< WordArticleLink >
  findArticles( const std::u32string &, bool ignoreDiacritics = false, uint32_t maxMatchCount = -1 );

  /// Find all unique article links in the index
  void findAllArticleLinks( QList< WordArticleLink > & articleLinks );

  /// Retrieve all unique headwords from index
  void getAllHeadwords( QSet< QString > & headwords );

  /// Find all article links and/or headwords in the index
  void findArticleLinks( QList< WordArticleLink > * articleLinks,
                         QSet< uint32_t > * offsets,
                         QSet< QString > * headwords,
                         QAtomicInt * isCancelled = 0 );



  /// Retrieve headwords for presented article addresses
  void
  getHeadwordsFromOffsets( QList< uint32_t > & offsets, QList< QString > & headwords, QAtomicInt * isCancelled = 0 );

protected:

  QMutex * idxFileMutex;
  File::Index * idxFile;

  MDB_env * env;
  MDB_dbi dbi;

private:
  void closeLmdb();
};

/// A base for the dictionary that utilizes a btree index build using
/// buildIndex() function declared below.
class BtreeDictionary: public Dictionary::Class, public BtreeIndex
{
public:

  BtreeDictionary( const string & id, const vector< string > & dictionaryFiles );

  /// Btree-indexed dictionaries are usually a good source for compound searches.
  virtual Dictionary::Features getFeatures() const noexcept
  {
    return Dictionary::SuitableForCompoundSearching;
  }

  /// This function does the search using the btree index. Derivatives usually
  /// need not to implement this function.
  virtual sptr< Dictionary::WordSearchRequest > prefixMatch( const std::u32string &, unsigned long );

  virtual sptr< Dictionary::WordSearchRequest >
  stemmedMatch( const std::u32string &, unsigned minLength, unsigned maxSuffixVariation, unsigned long maxResults );

  virtual bool isLocalDictionary()
  {
    return true;
  }

  virtual bool getHeadwords( QStringList & headwords );
  virtual void findHeadWordsWithLenth( QString & lastWord, QSet< QString > * headwords, uint32_t length );

  virtual void getArticleText( uint32_t articleAddress, QString & headword, QString & text );

  const string & ftsIndexName() const
  {
    return ftsIdxName;
  }

  QMutex & getFtsMutex()
  {
    return ftsIdxMutex;
  }

  virtual uint32_t getFtsIndexVersion()
  {
    return 0;
  }

  /// Called before each matching operation to ensure that any child init
  /// has completed. Mainly used for deferred init. The default implementation
  /// does nothing.
  /// The function returns an empty string if the initialization is or was
  /// successful, or a human-readable error string otherwise.
  virtual const string & ensureInitDone();

protected:
  QMutex ftsIdxMutex;
  string ftsIdxName;

  friend class BtreeWordSearchRequest;
  friend class FTSResultsRequest;
};

class BtreeWordSearchRequest: public Dictionary::WordSearchRequest
{
protected:
  BtreeDictionary & dict;
  std::u32string str;
  unsigned long maxResults;
  unsigned minLength;
  int maxSuffixVariation;
  bool allowMiddleMatches;
  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  BtreeWordSearchRequest( BtreeDictionary & dict_,
                          const std::u32string & str_,
                          unsigned minLength_,
                          int maxSuffixVariation_,
                          bool allowMiddleMatches_,
                          unsigned long maxResults_,
                          bool startRunnable = true );

  virtual void findMatches();

  void run();

  virtual void cancel()
  {
    isCancelled.ref();
  }

  ~BtreeWordSearchRequest();
};

// Everything below is for building the index data.

/// This represents the index in its source form, as a map which binds folded
/// words to sequences of their unfolded source forms and the corresponding
/// article offsets. The words are utf8-encoded -- it doesn't break Unicode
/// sorting, but conserves space.
struct IndexedWords: public map< string, vector< WordArticleLink > >
{
  /// Instead of adding to the map directly, use this function. It does folding
  /// itself, and for phrases/sentences it adds additional entries beginning with
  /// each new word.
  void addWord( const std::u32string & word, uint32_t articleOffset, unsigned int maxHeadwordSize = 100U );

  /// Differs from addWord() in that it only adds a single entry. We use this
  /// for zip's file names.
  void addSingleWord( const std::u32string & word, uint32_t articleOffset );
};

/// Builds the index, as a compressed btree. Returns IndexInfo.
/// All the data is stored to the given file, beginning from its current
/// position.
/// @param suffix Optional suffix for the LMDB file name to support multiple indices
IndexInfo buildIndex( const IndexedWords &, File::Index & file, const string & suffix = "" );

} // namespace BtreeIndexing
