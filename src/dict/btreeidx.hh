/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dict/dictionary.hh"
#include "dictfile.hh"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>
#include <QFuture>
#include <QList>


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
  FormatVersion = 4,
  //the indexedzip parse logic version
  ZipParseLogicVersion = 1
};

// These exceptions which might be thrown during the index traversal

DEF_EX( exIndexWasNotOpened, "The index wasn't opened", Dictionary::Ex )
DEF_EX( exFailedToDecompressNode, "Failed to decompress a btree's node", Dictionary::Ex )
DEF_EX( exCorruptedChainData, "Corrupted chain data in the leaf of a btree encountered", Dictionary::Ex )

/// This structure describes a word linked to its translation. The
/// translation is represented as an abstract 32-bit offset.
struct WordArticleLink
{
  string word, prefix; // in utf8
  uint32_t articleOffset = 0;

  WordArticleLink() = default;

  WordArticleLink( string const & word_, uint32_t articleOffset_, string const & prefix_ = string() ):
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

  IndexInfo( uint32_t btreeMaxElements_, uint32_t rootOffset_ ):
    btreeMaxElements( btreeMaxElements_ ),
    rootOffset( rootOffset_ )
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
  void openIndex( IndexInfo const &, File::Index &, QMutex & );

  /// Finds articles that match the given string. A case-insensitive search
  /// is performed.
  vector< WordArticleLink >
  findArticles( std::u32string const &, bool ignoreDiacritics = false, uint32_t maxMatchCount = -1 );

  /// Find all unique article links in the index
  void findAllArticleLinks( QList< WordArticleLink > & articleLinks );

  /// Retrieve all unique headwords from index
  void getAllHeadwords( QSet< QString > & headwords );

  /// Find all article links and/or headwords in the index
  void findArticleLinks( QList< WordArticleLink > * articleLinks,
                         QSet< uint32_t > * offsets,
                         QSet< QString > * headwords,
                         QAtomicInt * isCancelled = 0 );

  void findHeadWords( QList< uint32_t > offsets, int & index, QSet< QString > * headwords, uint32_t length );
  void findSingleNodeHeadwords( uint32_t offsets, QSet< QString > * headwords );
  QList< uint32_t > findNodes();

  /// Retrieve headwords for presented article addresses
  void
  getHeadwordsFromOffsets( QList< uint32_t > & offsets, QList< QString > & headwords, QAtomicInt * isCancelled = 0 );

protected:

  /// Finds the offset in the btree leaf for the given word, either matching
  /// by an exact match, or by finding the smallest entry that might match
  /// by prefix. It can return zero if there isn't even a possible prefx
  /// match. The input string must already be folded. The exactMatch is set
  /// to true when an exact match is located, and to false otherwise.
  /// The located leaf is loaded to 'leaf', and the pointer to the next
  /// leaf is saved to 'nextLeaf'.
  /// However, due to root node being permanently cached, the 'leaf' passed
  /// might not get used at all if the root node was the terminal one. In that
  /// case, the returned pointer wouldn't belong to 'leaf' at all. To that end,
  /// the leafEnd pointer always holds the pointer to the first byte outside
  /// the node data.
  char const * findChainOffsetExactOrPrefix( std::u32string const & target,
                                             bool & exactMatch,
                                             vector< char > & leaf,
                                             uint32_t & nextLeaf,
                                             char const *& leafEnd );

  /// Reads a node or leaf at the given offset. Just uncompresses its data
  /// to the given vector and does nothing more.
  void readNode( uint32_t offset, vector< char > & out );

  /// Reads the word-article links' chain at the given offset. The pointer
  /// is updated to point to the next chain, if there's any.
  vector< WordArticleLink > readChain( char const *&, uint32_t maxMatchCount = -1 );

  /// Drops any aliases which arose due to folding. Only case-folded aliases
  /// are left.
  void antialias( std::u32string const &, vector< WordArticleLink > &, bool ignoreDiactitics );

protected:

  QMutex * idxFileMutex;
  File::Index * idxFile;

private:

  uint32_t indexNodeSize;
  uint32_t rootOffset;
  bool rootNodeLoaded;
  vector< char > rootNode; // We load root note here and keep it at all times,
                           // since all searches always start with it.
};

/// A base for the dictionary that utilizes a btree index build using
/// buildIndex() function declared below.
class BtreeDictionary: public Dictionary::Class, public BtreeIndex
{
public:

  BtreeDictionary( string const & id, vector< string > const & dictionaryFiles );

  /// Btree-indexed dictionaries are usually a good source for compound searches.
  virtual Dictionary::Features getFeatures() const noexcept
  {
    return Dictionary::SuitableForCompoundSearching;
  }

  /// This function does the search using the btree index. Derivatives usually
  /// need not to implement this function.
  virtual sptr< Dictionary::WordSearchRequest > prefixMatch( std::u32string const &, unsigned long );

  virtual sptr< Dictionary::WordSearchRequest >
  stemmedMatch( std::u32string const &, unsigned minLength, unsigned maxSuffixVariation, unsigned long maxResults );

  virtual bool isLocalDictionary()
  {
    return true;
  }

  virtual bool getHeadwords( QStringList & headwords );
  virtual void findHeadWordsWithLenth( int &, QSet< QString > * headwords, uint32_t length );

  virtual void getArticleText( uint32_t articleAddress, QString & headword, QString & text );

  string const & ftsIndexName() const
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
  virtual string const & ensureInitDone();

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
                          std::u32string const & str_,
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
  void addWord( std::u32string const & word, uint32_t articleOffset, unsigned int maxHeadwordSize = 100U );

  /// Differs from addWord() in that it only adds a single entry. We use this
  /// for zip's file names.
  void addSingleWord( std::u32string const & word, uint32_t articleOffset );
};

/// Builds the index, as a compressed btree. Returns IndexInfo.
/// All the data is stored to the given file, beginning from its current
/// position.
IndexInfo buildIndex( IndexedWords const &, File::Index & file );

} // namespace BtreeIndexing
