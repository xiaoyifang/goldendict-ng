/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include <string.h>
#include "utils.hh"

#include <QRegularExpression>
#include "wildcard.hh"
#include "globalbroadcaster.hh"

#include <QtConcurrentRun>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUuid>
#include <rocksdb/convenience.h>
#include <zlib.h>

namespace BtreeIndexing {

using std::pair;

enum {
  BtreeMinElements = 64,
  BtreeMaxElements = 8192
};

BtreeIndex::BtreeIndex():
  idxFile( nullptr ),
  rootNodeLoaded( false ),
  headwordDb( nullptr ),
  headwordsCF( nullptr )
{
}

BtreeIndex::~BtreeIndex()
{
  if ( headwordDb )
  {
    if ( headwordsCF )
    {
      delete headwordsCF;
    }
    delete headwordDb;
  }
}

BtreeDictionary::BtreeDictionary( const string & id, const vector< string > & dictionaryFiles ):
  Dictionary::Class( id, dictionaryFiles )
{
}

const string & BtreeDictionary::ensureInitDone()
{
  static string empty;

  return empty;
}

void BtreeIndex::openIndex( const IndexInfo & indexInfo, File::Index & file, QMutex & mutex )
{
  indexNodeSize = indexInfo.btreeMaxElements;
  rootOffset    = indexInfo.rootOffset;

  idxFile      = &file;
  idxFileMutex = &mutex;

  rootNodeLoaded = false;
  rootNode.clear();
}

vector< WordArticleLink >
BtreeIndex::findArticles( const std::u32string & search_word, bool ignoreDiacritics, uint32_t maxMatchCount )
{
  //First trim ending zero
  std::u32string word = Text::removeTrailingZero( search_word );
  vector< WordArticleLink > result;

  try {
    std::u32string folded = Folding::apply( word );
    if ( folded.empty() ) {
      folded = Folding::applyWhitespaceOnly( word );
    }

    bool exactMatch;

    vector< char > leaf;
    uint32_t nextLeaf;

    const char * leafEnd;

    const char * chainOffset = findChainOffsetExactOrPrefix( folded, exactMatch, leaf, nextLeaf, leafEnd );

    if ( chainOffset && exactMatch ) {
      result = readChain( chainOffset, maxMatchCount );

      antialias( word, result, ignoreDiacritics );
    }
  }
  catch ( std::exception & e ) {
    qWarning( "Articles searching failed, error: %s", e.what() );
    result.clear();
  }
  catch ( ... ) {
    qWarning( "Articles searching failed" );
    result.clear();
  }

  return result;
}


BtreeWordSearchRequest::BtreeWordSearchRequest( BtreeDictionary & dict_,
                                                const std::u32string & str_,
                                                unsigned minLength_,
                                                int maxSuffixVariation_,
                                                bool allowMiddleMatches_,
                                                unsigned long maxResults_,
                                                bool startRunnable ):
  dict( dict_ ),
  str( str_ ),
  maxResults( maxResults_ ),
  minLength( minLength_ ),
  maxSuffixVariation( maxSuffixVariation_ ),
  allowMiddleMatches( allowMiddleMatches_ )
{
  if ( startRunnable ) {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }
}

void BtreeWordSearchRequest::findMatches()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  if ( dict.ensureInitDone().size() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  QRegularExpression regexp;

  bool useWildcards = false;
  if ( allowMiddleMatches ) {
    useWildcards = ( str.find( '*' ) != std::u32string::npos || str.find( '?' ) != std::u32string::npos
                     || str.find( '[' ) != std::u32string::npos || str.find( ']' ) != std::u32string::npos );
  }

  std::u32string folded = Folding::apply( str );

  int minMatchLength = 0;

  if ( useWildcards ) {
    regexp.setPattern( wildcardsToRegexp(
      QString::fromStdU32String( Folding::applyDiacriticsOnly( Folding::applySimpleCaseOnly( str ) ) ) ) );
    if ( !regexp.isValid() ) {
      regexp.setPattern( QRegularExpression::escape( regexp.pattern() ) );
    }
    regexp.setPatternOptions( QRegularExpression::CaseInsensitiveOption );

    bool bNoLetters = folded.empty();
    std::u32string foldedWithWildcards;

    if ( bNoLetters ) {
      foldedWithWildcards = Folding::applyWhitespaceOnly( str );
    }
    else {
      foldedWithWildcards = Folding::apply( str, useWildcards );
    }

    // Calculate minimum match length

    bool insideSet = false;
    bool escaped   = false;
    for ( char32_t ch : foldedWithWildcards ) {
      if ( ch == L'\\' && !escaped ) {
        escaped = true;
        continue;
      }

      if ( ch == L']' && !escaped ) {
        insideSet = false;
        continue;
      }

      if ( insideSet ) {
        escaped = false;
        continue;
      }

      if ( ch == L'[' && !escaped ) {
        minMatchLength += 1;
        insideSet = true;
        continue;
      }

      if ( ch == L'*' && !escaped ) {
        continue;
      }

      escaped = false;
      minMatchLength += 1;
    }

    // Fill first match chars

    folded.clear();
    folded.reserve( foldedWithWildcards.size() );
    escaped = false;
    for ( char32_t ch : foldedWithWildcards ) {
      if ( escaped ) {
        if ( bNoLetters || ( ch != L'*' && ch != L'?' && ch != L'[' && ch != L']' ) ) {
          folded.push_back( ch );
        }
        escaped = false;
        continue;
      }

      if ( ch == L'\\' ) {
        if ( bNoLetters || folded.empty() ) {
          escaped = true;
          continue;
        }
        else {
          break;
        }
      }

      if ( ch == '*' || ch == '?' || ch == '[' || ch == ']' ) {
        break;
      }

      folded.push_back( ch );
    }
  }
  else {
    if ( folded.empty() ) {
      folded = Folding::applyWhitespaceOnly( str );
    }
  }

  int initialFoldedSize = folded.size();

  int charsLeftToChop = 0;

  if ( maxSuffixVariation >= 0 ) {
    charsLeftToChop = initialFoldedSize - (int)minLength;

    if ( charsLeftToChop < 0 ) {
      charsLeftToChop = 0;
    }
    else if ( charsLeftToChop > maxSuffixVariation ) {
      charsLeftToChop = maxSuffixVariation;
    }
  }

  try {
    for ( ;; ) {
      bool exactMatch;
      vector< char > leaf;
      uint32_t nextLeaf;
      const char * leafEnd;

      const char * chainOffset = dict.findChainOffsetExactOrPrefix( folded, exactMatch, leaf, nextLeaf, leafEnd );

      if ( chainOffset ) {
        for ( ;; ) {
          if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
            break;
          }

          //qDebug( "offset = %u, size = %u", chainOffset - &leaf.front(), leaf.size() );

          vector< WordArticleLink > chain = dict.readChain( chainOffset );

          std::u32string chainHead = Text::toUtf32( chain[ 0 ].word );

          std::u32string resultFolded = Folding::apply( chainHead );
          if ( resultFolded.empty() ) {
            resultFolded = Folding::applyWhitespaceOnly( chainHead );
          }

          if ( ( useWildcards && folded.empty() )
               || ( resultFolded.size() >= folded.size() && !resultFolded.compare( 0, folded.size(), folded ) ) ) {
            // Exact or prefix match


            for ( auto & x : chain ) {
              if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
                break;
              }
              if ( useWildcards ) {
                std::u32string word   = Text::toUtf32( x.prefix + x.word );
                std::u32string result = Folding::applyDiacriticsOnly( word );
                if ( result.size() >= (std::u32string::size_type)minMatchLength ) {
                  QRegularExpressionMatch match = regexp.match( QString::fromStdU32String( result ) );
                  if ( match.hasMatch() && match.capturedStart() == 0 ) {
                    addMatch( word );
                  }
                }
              }
              else {
                // Skip middle matches, if requested. If suffix variation is specified,
                // make sure the string isn't larger than requested.
                if ( ( allowMiddleMatches || Folding::apply( Text::toUtf32( x.prefix ) ).empty() )
                     && ( maxSuffixVariation < 0
                          || (int)resultFolded.size() - initialFoldedSize <= maxSuffixVariation ) ) {
                  addMatch( Text::toUtf32( x.prefix + x.word ) );
                }
              }
              if ( matches.size() >= maxResults ) {
                break;
              }
            }

            if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
              break;
            }

            if ( matches.size() >= maxResults ) {
              break;
            }
          }
          else {
            // Neither exact nor a prefix match, end this
            break;
          }

          // Fetch new leaf if we're out of chains here

          if ( chainOffset >= leafEnd ) {
            // We're past the current leaf, fetch the next one

            //qDebug( "advancing" );

            if ( nextLeaf ) {
              QMutexLocker _( dict.idxFileMutex );

              dict.readNode( nextLeaf, leaf );
              leafEnd = &leaf.front() + leaf.size();

              nextLeaf    = dict.idxFile->read< uint32_t >();
              chainOffset = &leaf.front() + sizeof( uint32_t );

              uint32_t leafEntries = *(uint32_t *)&leaf.front();

              if ( leafEntries == 0xffffFFFF ) {
                //qDebug( "bah!" );
                exit( 1 );
              }
            }
            else {
              break; // That was the last leaf
            }
          }
        }
      }

      if ( charsLeftToChop && !Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        --charsLeftToChop;
        folded.resize( folded.size() - 1 );
      }
      else {
        break;
      }
    }
  }
  catch ( std::exception & e ) {
    qWarning( "Index searching failed: \"%s\", error: %s", dict.getName().c_str(), e.what() );
  }
  catch ( ... ) {
    qWarning( "Index searching failed: \"%s\"", dict.getName().c_str() );
  }
}

void BtreeWordSearchRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  if ( dict.ensureInitDone().size() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  findMatches();

  finish();
}

BtreeWordSearchRequest::~BtreeWordSearchRequest()
{
  isCancelled.ref();
  f.waitForFinished();
}

sptr< Dictionary::WordSearchRequest > BtreeDictionary::prefixMatch( const std::u32string & str,
                                                                    unsigned long maxResults )

{
  return std::make_shared< BtreeWordSearchRequest >( *this, str, 0, -1, true, maxResults );
}

sptr< Dictionary::WordSearchRequest > BtreeDictionary::stemmedMatch( const std::u32string & str,
                                                                     unsigned minLength,
                                                                     unsigned maxSuffixVariation,
                                                                     unsigned long maxResults )

{
  return std::make_shared< BtreeWordSearchRequest >( *this,
                                                     str,
                                                     minLength,
                                                     (int)maxSuffixVariation,
                                                     false,
                                                     maxResults );
}

void BtreeIndex::readNode( uint32_t offset, vector< char > & out )
{
  idxFile->seek( offset );

  uint32_t uncompressedSize = idxFile->read< uint32_t >();
  uint32_t compressedSize   = idxFile->read< uint32_t >();

  //qDebug( "%x,%x", uncompressedSize, compressedSize );

  out.resize( uncompressedSize );

  vector< unsigned char > compressedData( compressedSize );

  idxFile->read( &compressedData.front(), compressedData.size() );

  unsigned long decompressedLength = out.size();

  if ( uncompress( (unsigned char *)&out.front(), &decompressedLength, &compressedData.front(), compressedData.size() )
         != Z_OK
       || decompressedLength != out.size() ) {
    throw exFailedToDecompressNode();
  }
}

const char * BtreeIndex::findChainOffsetExactOrPrefix( const std::u32string & target,
                                                       bool & exactMatch,
                                                       vector< char > & extLeaf,
                                                       uint32_t & nextLeaf,
                                                       const char *& leafEnd )
{
  if ( !idxFile ) {
    throw exIndexWasNotOpened();
  }

  QMutexLocker _( idxFileMutex );

  // Lookup the index by traversing the index btree

  // vector< wchar > wcharBuffer;
  std::u32string w_word;
  exactMatch = false;

  // Read a node

  uint32_t currentNodeOffset = rootOffset;

  if ( !rootNodeLoaded ) {
    // Time to load our root node. We do it only once, at the first request.
    readNode( rootOffset, rootNode );
    rootNodeLoaded = true;
  }

  const char * leaf = &rootNode.front();
  leafEnd           = leaf + rootNode.size();

  if ( target.empty() ) {
    //For empty target string we return first chain in index
    for ( ;; ) {
      uint32_t leafEntries = *(uint32_t *)leaf;

      if ( leafEntries == 0xffffFFFF ) {
        // A node
        currentNodeOffset = *( (uint32_t *)leaf + 1 );
        readNode( currentNodeOffset, extLeaf );
        leaf     = &extLeaf.front();
        leafEnd  = leaf + extLeaf.size();
        nextLeaf = idxFile->read< uint32_t >();
      }
      else {
        // A leaf
        if ( currentNodeOffset == rootOffset ) {
          // Only one leaf in index, there's no next leaf
          nextLeaf = 0;
        }
        if ( !leafEntries ) {
          return nullptr;
        }

        return leaf + sizeof( uint32_t );
      }
    }
  }

  for ( ;; ) {
    // Is it a leaf or a node?

    uint32_t leafEntries = *(uint32_t *)leaf;

    if ( leafEntries == 0xffffFFFF ) {
      // A node

      //qDebug( "=>a node" );

      const uint32_t * offsets = (uint32_t *)leaf + 1;

      const char * ptr = leaf + sizeof( uint32_t ) + ( indexNodeSize + 1 ) * sizeof( uint32_t );

      // ptr now points to a span of zero-separated strings, up to leafEnd.
      // We find our match using a binary search.

      const char * closestString;

      int compareResult;

      const char * window = ptr;
      unsigned windowSize = leafEnd - ptr;

      for ( ;; ) {
        // We boldly shoot in the middle of the whole mess, and then adjust
        // to the beginning of the string that we've hit.
        const char * testPoint = window + windowSize / 2;

        closestString = testPoint;

        while ( closestString > ptr && closestString[ -1 ] ) {
          --closestString;
        }

        size_t wordSize = strlen( closestString );

        w_word = Text::toUtf32( string( closestString, wordSize ) );

        compareResult = target.compare( w_word );

        if ( !compareResult ) {
          // The target string matches the current one. Finish the search.
          break;
        }
        if ( compareResult < 0 ) {
          // The target string is smaller than the current one.
          // Go to the left.
          windowSize = closestString - window;

          if ( !windowSize ) {
            break;
          }
        }
        else {
          // The target string is larger than the current one.
          // Go to the right.
          windowSize -= ( closestString - window ) + wordSize + 1;
          window = closestString + wordSize + 1;

          if ( !windowSize ) {
            break;
          }
        }
      }


      // Now, whatever the outcome (compareResult) is, we need to find
      // entry number for the closestMatch string.

      unsigned entry = 0;

      for ( const char * next = ptr; next != closestString; next += strlen( next ) + 1, ++entry ) {
        ;
      }

      // Ok, now check the outcome

      if ( !compareResult ) {
        // The target string matches the one found.
        // Go to the right, since it's there where we store such results.
        currentNodeOffset = offsets[ entry + 1 ];
      }
      if ( compareResult < 0 ) {
        // The target string is smaller than the one found.
        // Go to the left.
        currentNodeOffset = offsets[ entry ];
      }
      else {
        // The target string is larger than the one found.
        // Go to the right.
        currentNodeOffset = offsets[ entry + 1 ];
      }

      //qDebug( "reading node at %x", currentNodeOffset );
      readNode( currentNodeOffset, extLeaf );
      leaf    = &extLeaf.front();
      leafEnd = leaf + extLeaf.size();
    }
    else {
      //qDebug( "=>a leaf" );
      // A leaf

      // If this leaf is the root, there's no next leaf, it just can't be.
      // We do this check because the file's position indicator just won't
      // be in the right place for root node anyway, since we precache it.
      nextLeaf = ( currentNodeOffset != rootOffset ? idxFile->read< uint32_t >() : 0 );

      if ( !leafEntries ) {
        // Empty leaf? This may only be possible for entirely empty trees only.
        if ( currentNodeOffset != rootOffset ) {
          throw exCorruptedChainData();
        }
        else {
          return nullptr; // No match
        }
      }

      // Build an array containing all chain pointers
      const char * ptr = leaf + sizeof( uint32_t );

      uint32_t chainSize;

      vector< const char * > chainOffsets( leafEntries );

      {
        const char ** nextOffset = &chainOffsets.front();

        while ( leafEntries-- ) {
          *nextOffset++ = ptr;

          memcpy( &chainSize, ptr, sizeof( uint32_t ) );

          //qDebug( "%s + %s", ptr + sizeof( uint32_t ), ptr + sizeof( uint32_t ) + strlen( ptr + sizeof( uint32_t ) ) + 1 );

          ptr += sizeof( uint32_t ) + chainSize;
        }
      }

      // Now do a binary search in it, aiming to find where our target
      // string lands.

      const char ** window = &chainOffsets.front();
      unsigned windowSize  = chainOffsets.size();

      for ( ;; ) {
        //qDebug( "window = %u, ws = %u", window - &chainOffsets.front(), windowSize );

        const char ** chainToCheck = window + windowSize / 2;
        ptr                        = *chainToCheck;

        memcpy( &chainSize, ptr, sizeof( uint32_t ) );
        ptr += sizeof( uint32_t );

        size_t wordSize = strlen( ptr );

        w_word = Text::toUtf32( string( ptr, wordSize ) );

        std::u32string foldedWord = Folding::apply( w_word );
        if ( foldedWord.empty() ) {
          foldedWord = Folding::applyWhitespaceOnly( w_word );
        }

        int compareResult = target.compare( foldedWord );

        if ( !compareResult ) {
          // Exact match -- return and be done
          exactMatch = true;

          return ptr - sizeof( uint32_t );
        }
        else if ( compareResult < 0 ) {
          // The target string is smaller than the current one.
          // Go to the first half

          windowSize /= 2;

          if ( !windowSize ) {
            // That finishes our search. Since our target string
            // landed before the last tested chain, we return a possible
            // prefix match against that chain.
            return ptr - sizeof( uint32_t );
          }
        }
        else {
          // The target string is larger than the current one.
          // Go to the second half

          windowSize -= windowSize / 2 + 1;

          if ( !windowSize ) {
            // That finishes our search. Since our target string
            // landed after the last tested chain, we return the next
            // chain. If there's no next chain in this leaf, this
            // would mean the first element in the next leaf.
            if ( chainToCheck == &chainOffsets.back() ) {
              if ( nextLeaf ) {
                readNode( nextLeaf, extLeaf );

                leafEnd = &extLeaf.front() + extLeaf.size();

                nextLeaf = idxFile->read< uint32_t >();

                return &extLeaf.front() + sizeof( uint32_t );
              }
              else {
                return nullptr; // This was the last leaf
              }
            }
            else {
              return chainToCheck[ 1 ];
            }
          }

          window = chainToCheck + 1;
        }
      }
    }
  }
}

vector< WordArticleLink > BtreeIndex::readChain( const char *& ptr, uint32_t maxMatchCount )
{
  uint32_t chainSize;

  memcpy( &chainSize, ptr, sizeof( uint32_t ) );

  ptr += sizeof( uint32_t );

  vector< WordArticleLink > result;

  while ( chainSize && ( maxMatchCount < 0 || result.size() < maxMatchCount ) ) {
    string str = ptr;
    ptr += str.size() + 1;

    string prefix = ptr;
    ptr += prefix.size() + 1;

    uint32_t articleOffset;

    memcpy( &articleOffset, ptr, sizeof( uint32_t ) );

    ptr += sizeof( uint32_t );

    result.emplace_back( str, articleOffset, prefix );

    if ( chainSize < str.size() + 1 + prefix.size() + 1 + sizeof( uint32_t ) ) {
      throw exCorruptedChainData();
    }
    else {
      chainSize -= str.size() + 1 + prefix.size() + 1 + sizeof( uint32_t );
    }
  }

  return result;
}

void BtreeIndex::antialias( const std::u32string & str, vector< WordArticleLink > & chain, bool ignoreDiacritics )
{
  std::u32string caseFolded = Folding::applySimpleCaseOnly( Text::normalize( str ) );
  if ( ignoreDiacritics ) {
    caseFolded = Folding::applyDiacriticsOnly( caseFolded );
  }

  if ( GlobalBroadcaster::instance()->getPreference()->ignorePunctuation ) {
    caseFolded = Folding::trimWhitespaceOrPunct( caseFolded );
  }

  for ( unsigned x = chain.size(); x--; ) {
    // If after applying case folding to each word they wouldn't match, we
    // drop the entry.
    std::u32string entry =
      Folding::applySimpleCaseOnly( Text::normalize( Text::toUtf32( chain[ x ].prefix + chain[ x ].word ) ) );
    if ( ignoreDiacritics ) {
      entry = Folding::applyDiacriticsOnly( entry );
    }

    if ( GlobalBroadcaster::instance()->getPreference()->ignorePunctuation ) {
      entry = Folding::trimWhitespaceOrPunct( entry );
    }

    if ( entry != caseFolded ) {
      chain.erase( chain.begin() + x );
    }
    else if ( !chain[ x ].prefix.empty() ) // If there's a prefix, merge it with the word,
                                           // since it's what dictionaries expect
    {
      chain[ x ].word.insert( 0, chain[ x ].prefix );
      chain[ x ].prefix.clear();
    }
  }
}


/// A function which recursively creates btree node.
/// The nextIndex iterator is being iterated over and increased when building
/// leaf nodes.
static uint32_t buildBtreeNode( IndexedWords::const_iterator & nextIndex,
                                size_t indexSize,
                                File::Index & file,
                                size_t maxElements,
                                uint32_t & lastLeafLinkOffset )
{
  // We compress all the node data. This buffer would hold it.
  vector< unsigned char > uncompressedData;

  bool isLeaf = indexSize <= maxElements;

  if ( isLeaf ) {
    // A leaf.

    uint32_t totalChainsLength = 0;

    auto nextWord = nextIndex;

    for ( unsigned x = indexSize; x--; ++nextWord ) {
      totalChainsLength += sizeof( uint32_t );

      const vector< WordArticleLink > & chain = nextWord->second;

      for ( const auto & y : chain ) {
        totalChainsLength += y.word.size() + 1 + y.prefix.size() + 1 + sizeof( uint32_t );
      }
    }

    uncompressedData.resize( sizeof( uint32_t ) + totalChainsLength );

    // First uint32_t indicates that this is a leaf.
    *(uint32_t *)&uncompressedData.front() = indexSize;

    unsigned char * ptr = &uncompressedData.front() + sizeof( uint32_t );

    for ( unsigned x = indexSize; x--; ++nextIndex ) {
      const vector< WordArticleLink > & chain = nextIndex->second;

      unsigned char * saveSizeHere = ptr;

      ptr += sizeof( uint32_t );

      uint32_t size = 0;

      for ( const auto & y : chain ) {
        memcpy( ptr, y.word.c_str(), y.word.size() + 1 );
        ptr += y.word.size() + 1;

        memcpy( ptr, y.prefix.c_str(), y.prefix.size() + 1 );
        ptr += y.prefix.size() + 1;

        memcpy( ptr, &( y.articleOffset ), sizeof( uint32_t ) );
        ptr += sizeof( uint32_t );

        size += y.word.size() + 1 + y.prefix.size() + 1 + sizeof( uint32_t );
      }

      memcpy( saveSizeHere, &size, sizeof( uint32_t ) );
    }
  }
  else {
    // A node which will have children.

    uncompressedData.resize( sizeof( uint32_t ) + ( maxElements + 1 ) * sizeof( uint32_t ) );

    // First uint32_t indicates that this is a node.
    *(uint32_t *)&uncompressedData.front() = 0xffffFFFF;

    unsigned prevEntry = 0;

    for ( unsigned x = 0; x < maxElements; ++x ) {
      unsigned curEntry = (uint64_t)indexSize * ( x + 1 ) / ( maxElements + 1 );

      uint32_t offset = buildBtreeNode( nextIndex, curEntry - prevEntry, file, maxElements, lastLeafLinkOffset );

      memcpy( &uncompressedData.front() + sizeof( uint32_t ) + x * sizeof( uint32_t ), &offset, sizeof( uint32_t ) );

      size_t sz = nextIndex->first.size() + 1;

      size_t prevSize = uncompressedData.size();
      uncompressedData.resize( prevSize + sz );

      memcpy( &uncompressedData.front() + prevSize, nextIndex->first.c_str(), sz );

      prevEntry = curEntry;
    }

    // Rightmost child
    uint32_t offset = buildBtreeNode( nextIndex, indexSize - prevEntry, file, maxElements, lastLeafLinkOffset );
    memcpy( &uncompressedData.front() + sizeof( uint32_t ) + maxElements * sizeof( uint32_t ),
            &offset,
            sizeof( offset ) );
  }

  // Save the result.
  vector< unsigned char > compressedData( compressBound( uncompressedData.size() ) );

  unsigned long compressedSize = compressedData.size();

  if ( compress( &compressedData.front(), &compressedSize, &uncompressedData.front(), uncompressedData.size() )
       != Z_OK ) {
    qFatal( "Failed to compress btree node." );
    abort();
  }

  uint32_t offset = file.tell();

  file.write< uint32_t >( uncompressedData.size() );
  file.write< uint32_t >( compressedSize );
  file.write( &compressedData.front(), compressedSize );

  if ( isLeaf ) {
    // A link to the next leef, which is zero and which will be updated
    // should we happen to have another leaf.

    file.write( (uint32_t)0 );

    uint32_t here = file.tell();

    if ( lastLeafLinkOffset ) {
      // Update the previous leaf to have the offset of this one.
      file.seek( lastLeafLinkOffset );
      file.write( offset );
      file.seek( here );
    }

    // Make sure next leaf knows where to write its offset for us.
    lastLeafLinkOffset = here - sizeof( uint32_t );
  }

  return offset;
}

void IndexedWords::addWord( const std::u32string & index_word, uint32_t articleOffset, unsigned int maxHeadwordSize )
{
  std::u32string word        = Text::removeTrailingZero( index_word );
  string::size_type wordSize = word.size();

  // Safeguard us against various bugs here. Don't attempt adding words
  // which are freakishly huge.
  if ( wordSize > maxHeadwordSize ) {
    qWarning() << "Abbreviate the too long headword: " << QString::fromStdU32String( word.substr( 0, 30 ) )
               << "size:" << wordSize;

    //find the closest string to the maxHeadwordSize;
    auto nonSpacePos = word.find_last_not_of( ' ', maxHeadwordSize );
    if ( nonSpacePos > 0 ) {
      word = word.substr( 0, nonSpacePos );
    }
    else {
      word = word.substr( 0, maxHeadwordSize );
    }

    wordSize = word.size();
  }
  const char32_t * wordBegin = word.c_str();

  // Skip any leading whitespace
  while ( *wordBegin && Folding::isWhitespace( *wordBegin ) ) {
    ++wordBegin;
    --wordSize;
  }

  // Skip any trailing whitespace
  while ( wordSize && Folding::isWhitespace( wordBegin[ wordSize - 1 ] ) ) {
    --wordSize;
  }

  const char32_t * nextChar = wordBegin;

  vector< char > utfBuffer( wordSize * 4 );

  int wordsAdded = 0; // Number of stored parts

  for ( ;; ) {
    // Skip any whitespace/punctuation
    for ( ;; ++nextChar ) {
      if ( !*nextChar ) // End of string ends everything
      {
        if ( wordsAdded == 0 ) {
          std::u32string folded = Folding::applyWhitespaceOnly( std::u32string( wordBegin, wordSize ) );
          if ( !folded.empty() ) {
            string utfWord = Text::toUtf8( std::u32string( wordBegin, wordSize ) );
            string utfPrefix;
            // Write to index column family
            db->Merge(write_options, indexCF, Text::toUtf8(folded), serializeLink({utfWord, articleOffset, utfPrefix}));
            // Write to headwords column family (only when prefix is empty)
            if (headwordsCF) {
              db->Put(write_options, headwordsCF, Text::toUtf8(folded), utfWord);
            }
          }
        }
        return;
      }

      if ( !Folding::isWhitespace( *nextChar ) && !Folding::isPunct( *nextChar ) ) {
        break;
      }
    }

    // Insert this word
    std::u32string folded = Folding::apply( nextChar );
    auto name             = Text::toUtf8( folded );
    
    string utfWord   = Text::toUtf8( std::u32string( nextChar, wordSize - ( nextChar - wordBegin ) ) );
    string utfPrefix = Text::toUtf8( std::u32string( wordBegin, nextChar - wordBegin ) );

    // Write to index column family
    db->Merge(write_options, indexCF, name, serializeLink({utfWord, articleOffset, utfPrefix}));
    
    // Write to headwords column family only if prefix is empty (first word)
    if (headwordsCF && utfPrefix.empty()) {
      // Store the complete original headword
      string completeHeadword = Text::toUtf8( std::u32string( wordBegin, wordSize ) );
      db->Put(write_options, headwordsCF, name, completeHeadword);
    }

    wordsAdded += 1;

    // Skip all non-whitespace/punctuation
    for ( ++nextChar;; ++nextChar ) {
      if ( !*nextChar ) {
        return; // End of string ends everything
      }

      if ( Folding::isWhitespace( *nextChar ) || Folding::isPunct( *nextChar ) ) {
        break;
      }
    }
  }
}

IndexedWords::IndexedWords() : db(nullptr), indexCF(nullptr), headwordsCF(nullptr), isPersistent(false) {
  QTemporaryDir tempDir;
  if (tempDir.isValid()) {
    db_path = tempDir.path().toStdString() + QDir::separator().toLatin1() + QUuid::createUuid().toString().toStdString();
  } else {
    db_path = (QDir::tempPath() + QDir::separator() + QUuid::createUuid().toString()).toStdString();
  }

  rocksdb::Options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new WordArticleLinkMergeOperator());
  // Optimize for bulk loading
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();

  // Define Column Families
  std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
  column_families.push_back(rocksdb::ColumnFamilyDescriptor(
      rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions()));
  column_families.push_back(rocksdb::ColumnFamilyDescriptor(
      "headwords", rocksdb::ColumnFamilyOptions()));

  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  rocksdb::Status status = rocksdb::DB::Open(options, db_path, column_families, &handles, &db);
  
  if (!status.ok()) {
    qWarning() << "Failed to open RocksDB database at " << QString::fromStdString(db_path) 
               << ": " << status.ToString().c_str();
    db = nullptr;
    indexCF = nullptr;
    headwordsCF = nullptr;
  } else {
    indexCF = handles[0];      // default column family for index
    headwordsCF = handles[1];  // headwords column family
  }
  
  write_options.disableWAL = true; // Faster writes for bulk loading
}

IndexedWords::~IndexedWords() {
  if (db) {
    // Close column family handles
    if (indexCF) {
      delete indexCF;
      indexCF = nullptr;
    }
    if (headwordsCF) {
      delete headwordsCF;
      headwordsCF = nullptr;
    }
    
    // Close database
    delete db;
    db = nullptr;
    
    // Only destroy database if it's temporary (not persistent)
    if (!isPersistent) {
      rocksdb::DestroyDB(db_path, rocksdb::Options());
    }
  }
}

rocksdb::Iterator* IndexedWords::newIterator() {
  if (!db || !indexCF) return nullptr;
  rocksdb::ReadOptions read_options;
  read_options.fill_cache = false; // Don't pollute cache during bulk read
  return db->NewIterator(read_options, indexCF);
}

uint64_t IndexedWords::size() {
  if (!db) return 0;
  uint64_t num_keys = 0;
  db->GetAggregatedIntProperty("rocksdb.estimate-num-keys", &num_keys);
  return num_keys;
}

bool IndexedWords::empty() {
  return size() == 0;
}

void IndexedWords::clear() {
  if (db) {
    delete db;
    db = nullptr;
    rocksdb::DestroyDB(db_path, rocksdb::Options());

    rocksdb::Options options;
    options.create_if_missing = true;
    options.merge_operator.reset(new WordArticleLinkMergeOperator());
    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
      qWarning() << "Failed to reopen RocksDB database at " << QString::fromStdString(db_path) << ": " << status.ToString().c_str();
      db = nullptr;
    }
  }
}

void IndexedWords::clear()
{
  // intentionally left blank, rocksdb will auto clean in destructor.
}

void IndexedWords::dropIndexFamily() {
  if (!db || !indexCF) {
    return;
  }
  
  qDebug() << "Dropping index column family to save disk space...";
  
  // Drop the index column family
  rocksdb::Status status = db->DropColumnFamily(indexCF);
  if (!status.ok()) {
    qWarning() << "Failed to drop index column family: " << status.ToString().c_str();
    return;
  }
  
  // Delete the column family handle
  delete indexCF;
  indexCF = nullptr;
  
  qDebug() << "Index column family dropped successfully";
}

bool IndexedWords::moveToPersistent(const std::string& newPath) {
  if (!db || isPersistent) {
    return false;
  }

  // Close everything first
  if (indexCF) {
    delete indexCF;
    indexCF = nullptr;
  }
  if (headwordsCF) {
    delete headwordsCF;
    headwordsCF = nullptr;
  }
  delete db;
  db = nullptr;

  // Move the directory
  if (QFile::exists(QString::fromStdString(newPath))) {
      QDir(QString::fromStdString(newPath)).removeRecursively();
  }
  
  if (!QFile::rename(QString::fromStdString(db_path), QString::fromStdString(newPath))) {
      qWarning() << "Failed to move RocksDB from " << QString::fromStdString(db_path) 
                 << " to " << QString::fromStdString(newPath);
      // Try to recover? For now getting back is hard without re-opening.
      // But we shouldn't fail silently.
      return false;
  }

  // Update state
  // QTemporaryDir destructor will try to remove the old path, but it's gone.
  // We don't need to do anything about QTemporaryDir object since it's local to constructor.
  // Wait, QTemporaryDir was local to constructor. It already tried to remove it when constructor finished.
  // But we determined that the logic in constructor used a text path derived from it.
  
  db_path = newPath;
  isPersistent = true;
  
  return true;
}

string IndexedWords::serializeLink(const WordArticleLink& link) {
    string serialized;
    serialized.reserve(sizeof(uint32_t) * 2 + link.word.size() + link.prefix.size() + sizeof(uint32_t));
    uint32_t word_len = link.word.size();
    uint32_t prefix_len = link.prefix.size();
    serialized.append(reinterpret_cast<const char*>(&word_len), sizeof(word_len));
    serialized.append(link.word);
    serialized.append(reinterpret_cast<const char*>(&prefix_len), sizeof(prefix_len));
    serialized.append(link.prefix);
    serialized.append(reinterpret_cast<const char*>(&link.articleOffset), sizeof(link.articleOffset));
    return serialized;
}

vector<WordArticleLink> IndexedWords::deserializeChain(const rocksdb::Slice& slice) {
    vector<WordArticleLink> chain;
    const char* ptr = slice.data();
    const char* end = ptr + slice.size();
    while (ptr < end) {
        uint32_t word_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += sizeof(uint32_t);
        string word(ptr, word_len); ptr += word_len;
        uint32_t prefix_len = *reinterpret_cast<const uint32_t*>(ptr); ptr += sizeof(uint32_t);
        string prefix(ptr, prefix_len); ptr += prefix_len;
        uint32_t articleOffset = *reinterpret_cast<const uint32_t*>(ptr); ptr += sizeof(uint32_t);
        chain.emplace_back(word, articleOffset, prefix);
    }
    return chain;
}

void IndexedWords::addSingleWord( const std::u32string & index_word, uint32_t articleOffset )
{
  const std::u32string & word = Text::removeTrailingZero( index_word );
  std::u32string folded       = Folding::apply( word );
  if ( folded.empty() ) {
    folded = Folding::applyWhitespaceOnly( word );
  }
  string foldedUtf8 = Text::toUtf8(folded);
  string wordUtf8 = Text::toUtf8(word);
  
  // Write to index column family
  db->Merge(write_options, indexCF, foldedUtf8, serializeLink({wordUtf8, articleOffset}));
  
  // Write to headwords column family (single words always have empty prefix)
  if (headwordsCF) {
    db->Put(write_options, headwordsCF, foldedUtf8, wordUtf8);
  }
}

IndexInfo buildIndex( IndexedWords & indexedWords, File::Index & file )
{
  size_t indexSize = indexedWords.size();
  sptr<rocksdb::Iterator> it(indexedWords.newIterator());

  // Skip any empty words. No point in indexing those, and some dictionaries
  // are known to have buggy empty-word entries (Stardict's jargon for instance).
  it->SeekToFirst();
  while (indexSize && it->Valid() && it->key().empty()) {
    indexSize--;
    it->Next();
  }

  // We try to stick to two-level tree for most dictionaries. Try finding
  // the right size for it.

  size_t btreeMaxElements = ( (size_t)sqrt( (double)indexSize ) ) + 1;

  if ( btreeMaxElements < BtreeMinElements ) {
    btreeMaxElements = BtreeMinElements;
  }
  else if ( btreeMaxElements > BtreeMaxElements ) {
    btreeMaxElements = BtreeMaxElements;
  }

  qDebug( "Building a tree of %u elements", (unsigned)btreeMaxElements );


  uint32_t lastLeafOffset = 0;

  uint32_t rootOffset = buildBtreeNode( it, indexSize, file, btreeMaxElements, lastLeafOffset );

  // After btree is built, drop the index column family to save disk space
  // The headwords column family will be retained for querying
  indexedWords.dropIndexFamily();

  // Persist the RocksDB query index
  string rocksDbPath = file.fileName().toStdString() + ".rocksdb";
  indexedWords.moveToPersistent(rocksDbPath);

  return IndexInfo( btreeMaxElements, rootOffset );
}

void BtreeIndex::getAllHeadwords( QSet< QString > & headwords )
{
  if ( !idxFile ) {
    throw exIndexWasNotOpened();
  }

  findArticleLinks( nullptr, nullptr, &headwords );
}

void BtreeIndex::findAllArticleLinks( QList< WordArticleLink > & articleLinks )
{
  if ( !idxFile ) {
    throw exIndexWasNotOpened();
  }

  QSet< uint32_t > offsets;

  findArticleLinks( &articleLinks, &offsets, nullptr );
}

void BtreeIndex::findArticleLinks( QList< WordArticleLink > * articleLinks,
                                   QSet< uint32_t > * offsets,
                                   QSet< QString > * headwords,
                                   QAtomicInt * isCancelled )
{
  uint32_t currentNodeOffset = rootOffset;
  uint32_t nextLeaf          = 0;
  uint32_t leafEntries;

  QMutexLocker _( idxFileMutex );

  if ( !rootNodeLoaded ) {
    // Time to load our root node. We do it only once, at the first request.
    readNode( rootOffset, rootNode );
    rootNodeLoaded = true;
  }

  const char * leaf     = &rootNode.front();
  const char * leafEnd  = leaf + rootNode.size();
  const char * chainPtr = nullptr;

  vector< char > extLeaf;

  // Find first leaf

  for ( ;; ) {
    leafEntries = *(uint32_t *)leaf;

    if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) {
      return;
    }

    if ( leafEntries == 0xffffFFFF ) {
      // A node
      currentNodeOffset = *( (uint32_t *)leaf + 1 );
      readNode( currentNodeOffset, extLeaf );
      leaf     = &extLeaf.front();
      leafEnd  = leaf + extLeaf.size();
      nextLeaf = idxFile->read< uint32_t >();
    }
    else {
      // A leaf
      chainPtr = leaf + sizeof( uint32_t );
      break;
    }
  }

  if ( !leafEntries ) {
    // Empty leaf? This may only be possible for entirely empty trees only.
    if ( currentNodeOffset != rootOffset ) {
      throw exCorruptedChainData();
    }
    else {
      return; // No match
    }
  }

  // Read all chains

  for ( ;; ) {
    vector< WordArticleLink > result = readChain( chainPtr );

    for ( auto & i : result ) {
      if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) {
        return;
      }

      if ( headwords ) {
        headwords->insert( QString::fromUtf8( ( i.prefix + i.word ).c_str() ) );
      }

      if ( offsets && offsets->contains( i.articleOffset ) ) {
        continue;
      }

      if ( offsets ) {
        offsets->insert( i.articleOffset );
      }

      if ( articleLinks ) {
        articleLinks->push_back( WordArticleLink( i.prefix + i.word, i.articleOffset ) );
      }
    }

    if ( chainPtr >= leafEnd ) {
      // We're past the current leaf, fetch the next one

      if ( nextLeaf ) {
        readNode( nextLeaf, extLeaf );
        leaf    = &extLeaf.front();
        leafEnd = leaf + extLeaf.size();

        nextLeaf = idxFile->read< uint32_t >();
        chainPtr = leaf + sizeof( uint32_t );

        leafEntries = *(uint32_t *)leaf;

        if ( leafEntries == 0xffffFFFF ) {
          throw exCorruptedChainData();
        }
      }
      else {
        break; // That was the last leaf
      }
    }
  }
}

void BtreeIndex::findHeadWords( QList< uint32_t > offsets, int & index, QSet< QString > * headwords, uint32_t length )
{
  for ( auto begin = offsets.begin() + index; begin != offsets.end(); begin++ ) {
    findSingleNodeHeadwords( *begin, headwords );
    index++;

    if ( headwords->size() >= length ) {
      break;
    }
  }
}

void BtreeIndex::findSingleNodeHeadwords( uint32_t offsets, QSet< QString > * headwords )
{
  uint32_t currentNodeOffset = offsets;

  QMutexLocker _( idxFileMutex );

  const char * leaf     = nullptr;
  const char * leafEnd  = nullptr;
  const char * chainPtr = nullptr;

  vector< char > extLeaf;

  // A node
  readNode( currentNodeOffset, extLeaf );
  leaf    = &extLeaf.front();
  leafEnd = leaf + extLeaf.size();

  // A leaf
  chainPtr = leaf + sizeof( uint32_t );

  for ( ;; ) {
    vector< WordArticleLink > result = readChain( chainPtr );

    if ( headwords ) {
      for ( auto & i : result ) {
        // Only add when prefix is empty
        if ( i.prefix.empty() ) {
          headwords->insert( QString::fromUtf8( ( i.prefix + i.word ).c_str() ) );
        }
      }
    }

    if ( chainPtr >= leafEnd ) {
      break; // That was the last leaf
    }
  }
}

//find the next chain ptr ,which is larger than this currentChainPtr
QList< uint32_t > BtreeIndex::findNodes()
{
  QMutexLocker _( idxFileMutex );

  if ( !rootNodeLoaded ) {
    // Time to load our root node. We do it only once, at the first request.
    readNode( rootOffset, rootNode );
    rootNodeLoaded = true;
  }

  const char * leaf = &rootNode.front();
  QList< uint32_t > leafOffset;

  uint32_t leafEntries;
  leafEntries = *(uint32_t *)leaf;
  if ( leafEntries != 0xffffFFFF ) {
    leafOffset.append( rootOffset );
    return leafOffset;
  }

  // the current the btree's implementation has the  height = 2.

  // A node offset
  uint32_t * offsets = (uint32_t *)leaf + 1;
  uint32_t i         = 0;

  while ( i++ < ( indexNodeSize + 1 ) ) {
    leafOffset.append( *( offsets++ ) );
  }

  return leafOffset;
}

void BtreeIndex::getHeadwordsFromOffsets( QList< uint32_t > & offsets,
                                          QList< QString > & headwords,
                                          QAtomicInt * isCancelled )
{
  uint32_t currentNodeOffset = rootOffset;
  uint32_t nextLeaf          = 0;
  uint32_t leafEntries;

  std::sort( offsets.begin(), offsets.end() );

  QMutexLocker _( idxFileMutex );

  if ( !rootNodeLoaded ) {
    // Time to load our root node. We do it only once, at the first request.
    readNode( rootOffset, rootNode );
    rootNodeLoaded = true;
  }

  const char * leaf     = &rootNode.front();
  const char * leafEnd  = leaf + rootNode.size();
  const char * chainPtr = nullptr;

  vector< char > extLeaf;

  // Find first leaf

  for ( ;; ) {
    leafEntries = *(uint32_t *)leaf;

    if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) {
      return;
    }

    if ( leafEntries == 0xffffFFFF ) {
      // A node
      currentNodeOffset = *( (uint32_t *)leaf + 1 );
      readNode( currentNodeOffset, extLeaf );
      leaf     = &extLeaf.front();
      leafEnd  = leaf + extLeaf.size();
      nextLeaf = idxFile->read< uint32_t >();
    }
    else {
      // A leaf
      chainPtr = leaf + sizeof( uint32_t );
      break;
    }
  }

  if ( !leafEntries ) {
    // Empty leaf? This may only be possible for entirely empty trees only.
    if ( currentNodeOffset != rootOffset ) {
      throw exCorruptedChainData();
    }
    else {
      return; // No match
    }
  }

  // Read all chains

  QList< uint32_t >::Iterator begOffsets = offsets.begin();
  QList< uint32_t >::Iterator endOffsets = offsets.end();

  for ( ;; ) {
    vector< WordArticleLink > result = readChain( chainPtr );

    for ( auto & i : result ) {
      uint32_t articleOffset = i.articleOffset;

      QList< uint32_t >::Iterator it = std::lower_bound( begOffsets, endOffsets, articleOffset );

      if ( it != offsets.end() && *it == articleOffset ) {
        if ( isCancelled && Utils::AtomicInt::loadAcquire( *isCancelled ) ) {
          return;
        }

        auto word = QString::fromUtf8( ( i.prefix + i.word ).c_str() );

        if ( headwords.indexOf( word ) == -1 ) {
          headwords.append( word );
        }
        offsets.erase( it );
        begOffsets = offsets.begin();
        endOffsets = offsets.end();
      }

      if ( offsets.isEmpty() ) {
        break;
      }
    }

    if ( offsets.isEmpty() ) {
      break;
    }

    if ( chainPtr >= leafEnd ) {
      // We're past the current leaf, fetch the next one

      if ( nextLeaf ) {
        readNode( nextLeaf, extLeaf );
        leaf    = &extLeaf.front();
        leafEnd = leaf + extLeaf.size();

        nextLeaf = idxFile->read< uint32_t >();
        chainPtr = leaf + sizeof( uint32_t );

        leafEntries = *(uint32_t *)leaf;

        if ( leafEntries == 0xffffFFFF ) {
          throw exCorruptedChainData();
        }
      }
      else {
        break; // That was the last leaf
      }
    }
  }
}

bool BtreeDictionary::getHeadwords( QStringList & headwords )
{
  QSet< QString > setOfHeadwords;

  headwords.clear();
  setOfHeadwords.reserve( getWordCount() );

  try {
    getAllHeadwords( setOfHeadwords );

    if ( setOfHeadwords.size() ) {
      headwords.reserve( setOfHeadwords.size() );

      QSet< QString >::const_iterator it  = setOfHeadwords.constBegin();
      QSet< QString >::const_iterator end = setOfHeadwords.constEnd();

      for ( ; it != end; ++it ) {
        headwords.append( *it );
      }
    }
  }
  catch ( std::exception & ex ) {
    qWarning( "Failed headwords retrieving for \"%s\", reason: %s", getName().c_str(), ex.what() );
  }

  return headwords.size() > 0;
}

void BtreeDictionary::findHeadWordsWithLenth( int & index, QSet< QString > * headwords, uint32_t length )
{
  auto leafNodeOffsets = findNodes();
  findHeadWords( leafNodeOffsets, index, headwords, length );
}

void BtreeDictionary::getArticleText( uint32_t, QString &, QString & ) {}

bool BtreeIndex::openHeadwordIndex( const std::string & rocksDbPath )
{
  if ( headwordDb ) {
    return true; // Already open
  }

  // Check if the directory exists
  if ( !QFileInfo( QString::fromStdString( rocksDbPath ) ).exists() ) {
    return false;
  }

  rocksdb::Options options;
  
  // We need to list column families and open them
  std::vector<std::string> column_families;
  rocksdb::Status status = rocksdb::DB::ListColumnFamilies(rocksdb::DBOptions(), rocksDbPath, &column_families);
  if (!status.ok()) {
     // This might happen if the DB was created without column families (legacy/temp)
     // or if path is invalid. But we checked existence.
     // Also, ListColumnFamilies requires the DB to exist.
     return false;
  }
  
  std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
  for (const auto& cf_name : column_families) {
    descriptors.emplace_back(cf_name, rocksdb::ColumnFamilyOptions());
  }

  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  status = rocksdb::DB::OpenForReadOnly(rocksdb::DBOptions(), rocksDbPath, descriptors, &handles, &headwordDb);
  
  if (!status.ok()) {
    qWarning() << "Failed to open RocksDB for reading at " << QString::fromStdString(rocksDbPath) << ": " << status.ToString().c_str();
    return false;
  }
  
  // Find the headwords column family handle
  for (size_t i = 0; i < column_families.size(); ++i) {
    if (column_families[i] == "headwords") {
      headwordsCF = handles[i];
    } else {
        // We don't need other handles, but we must delete them
        delete handles[i];
    }
  }
  
  if (!headwordsCF) {
      // "headwords" family not found? Then we can't use it.
      // This implies an incomplete DB or old format.
      delete headwordDb; 
      headwordDb = nullptr;
      return false;
  }
  
  return true;
}

QStringList BtreeIndex::getHeadwordsWithPrefix( const std::string & prefix )
{
    QStringList results;
    if (!headwordDb || !headwordsCF) {
        return results;
    }

    rocksdb::ReadOptions read_options;
    // Optimize for prefix seek?
    
    std::unique_ptr<rocksdb::Iterator> it(headwordDb->NewIterator(read_options, headwordsCF));
    
    rocksdb::Slice prefix_slice(prefix);
    
    if (prefix.empty()) {
        it->SeekToFirst();
    } else {
        it->Seek(prefix_slice);
    }

    for (; it->Valid(); it->Next()) {
        // Check if key starts with prefix
        if (!prefix.empty()) {
           rocksdb::Slice key = it->key();
           if (!key.starts_with(prefix_slice)) {
               break;
           }
        }
        
        // Extract value (original headword)
        std::string val = it->value().ToString();
        results.append(QString::fromUtf8(val.c_str()));
    }
    
    return results;
}

} // namespace BtreeIndexing
