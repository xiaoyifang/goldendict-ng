/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "wordfinder.hh"
#include "folding.hh"
#include <map>
#include <QMutexLocker>


using std::vector;
using std::list;
using std::map;
using std::pair;

WordFinder::WordFinder( QObject * parent ):
  QObject( parent ),
  searchInProgress( false ),
  updateResultsTimer( this )
{
  updateResultsTimer.setInterval( 1000 ); // We use a one second update timer
  updateResultsTimer.setSingleShot( true );

  connect( &updateResultsTimer, &QTimer::timeout, this, &WordFinder::updateResults, Qt::QueuedConnection );
}

WordFinder::~WordFinder()
{
  clear();
}

void WordFinder::prefixMatch( QString const & str,
                              std::vector< sptr< Dictionary::Class > > const & dicts,
                              unsigned long maxResults,
                              Dictionary::Features features )
{
  cancel();

  searchType          = PrefixMatch;
  inputWord           = str;
  inputDicts          = &dicts;
  requestedMaxResults = maxResults;
  requestedFeatures   = features;

  resultsArray.clear();
  resultsIndex.clear();
  searchResults.clear();

  // queuedRequests is empty, so we can safely call startSearch()
  startSearch();
}
void WordFinder::stemmedMatch( QString const & str,
                               std::vector< sptr< Dictionary::Class > > const & dicts,
                               unsigned minLength,
                               unsigned maxSuffixVariation,
                               unsigned long maxResults,
                               Dictionary::Features features )
{
  cancel();

  searchType                = StemmedMatch;
  inputWord                 = str;
  inputDicts                = &dicts;
  requestedMaxResults       = maxResults;
  requestedFeatures         = features;
  stemmedMinLength          = minLength;
  stemmedMaxSuffixVariation = maxSuffixVariation;

  resultsArray.clear();
  resultsIndex.clear();
  searchResults.clear();

  startSearch();
}

void WordFinder::expressionMatch( QString const & str,
                                  std::vector< sptr< Dictionary::Class > > const & dicts,
                                  unsigned long maxResults,
                                  Dictionary::Features features )
{
  cancel();

  searchType          = ExpressionMatch;
  inputWord           = str;
  inputDicts          = &dicts;
  requestedMaxResults = maxResults;
  requestedFeatures   = features;

  resultsArray.clear();
  resultsIndex.clear();
  searchResults.clear();

  startSearch();
}

void WordFinder::startSearch()
{
  if ( searchInProgress.load() ) {
    return; // Search was probably processing
  }

  {
    // Clear the requests just in case
    queuedRequests.clear();

    searchErrorString.clear();
    searchResultsUncertain = false;

    searchInProgress = true;
  }

  updateResultsTimer.start();

  // Gather all writings of the word

  if ( allWordWritings.size() != 1 ) {
    allWordWritings.resize( 1 );
  }

  allWordWritings[ 0 ] = inputWord.toStdU32String();

  for ( const auto & inputDict : *inputDicts ) {
    vector< std::u32string > writings = inputDict->getAlternateWritings( allWordWritings[ 0 ] );

    allWordWritings.insert( allWordWritings.end(), writings.begin(), writings.end() );
  }

  // Query each dictionary for all word writings

  for ( const auto & inputDict : *inputDicts ) {
    if ( ( inputDict->getFeatures() & requestedFeatures ) != requestedFeatures ) {
      continue;
    }

    for ( const auto & allWordWriting : allWordWritings ) {
      try {
        sptr< Dictionary::WordSearchRequest > sr = ( searchType == PrefixMatch || searchType == ExpressionMatch ) ?
          inputDict->prefixMatch( allWordWriting, requestedMaxResults ) :
          inputDict->stemmedMatch( allWordWriting, stemmedMinLength, stemmedMaxSuffixVariation, requestedMaxResults );

        connect( sr.get(), &Dictionary::Request::finished, this, [ this ]() {
          requestFinished();
        } );

        queuedRequests.push_back( sr );
      }
      catch ( std::exception & e ) {
        qWarning( "Word \"%s\" search error (%s) in \"%s\"",
                  inputWord.toUtf8().data(),
                  e.what(),
                  inputDict->getName().c_str() );
      }
    }
  }

  // Handle any requests finished already

  requestFinished();
}

void WordFinder::cancel()
{
  searchInProgress = false;

  cancelSearches();
}

void WordFinder::clear()
{
  cancel();

  queuedRequests.clear();
}

void WordFinder::requestFinished()
{
  if ( !searchInProgress.load() ) {
    return;
  }
  QMutexLocker locker( &mutex );
  // See how many new requests have finished, and if we have any new results
  // Create a snapshot of queuedRequests to avoid iterator invalidation
  auto snapshot = queuedRequests.snapshot();

  bool all_finished = true;
  // Iterate over the snapshot
  for ( const auto & request : snapshot ) {
    // Break the loop if the search is no longer in progress
    if ( !searchInProgress.load() ) {
      return;
    }

    // Check if the request is finished
    if ( !request->isFinished() ) {
      all_finished = false;
      break;
    }
  }

  if ( !searchInProgress.load() ) {
    return;
  }

  if ( all_finished ) {
    // Search is finished.
    updateResults();
  }
}

namespace {


unsigned saturated( unsigned x )
{
  return x < 255 ? x : 255;
}

/// Checks whether the first string has the second one inside, surrounded from
/// both sides by either whitespace, punctuation or begin/end of string.
/// If true is returned, pos holds the offset in the haystack. If the offset
/// is larger than 255, it is set to 255.
bool hasSurroundedWithWs( std::u32string const & haystack,
                          std::u32string const & needle,
                          std::u32string::size_type & pos )
{
  if ( haystack.size() < needle.size() ) {
    return false; // Needle won't even fit into a haystack
  }

  for ( pos = 0;; ++pos ) {
    pos = haystack.find( needle, pos );

    if ( pos == std::u32string::npos ) {
      return false; // Not found
    }

    if ( ( !pos || Folding::isWhitespace( haystack[ pos - 1 ] ) || Folding::isPunct( haystack[ pos - 1 ] ) )
         && ( ( pos + needle.size() == haystack.size() ) || Folding::isWhitespace( haystack[ pos + needle.size() ] )
              || Folding::isPunct( haystack[ pos + needle.size() ] ) ) ) {
      pos = saturated( pos );

      return true;
    }
  }
}

} // namespace



int WordFinder::levenshteinDistance(const std::u32string &s1, const std::u32string &s2) {
  int len1 = s1.size();
  int len2 = s2.size();
  std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1));

  for (int i = 0; i <= len1; ++i) {
      dp[i][0] = i;
  }
  for (int j = 0; j <= len2; ++j) {
      dp[0][j] = j;
  }

  for (int i = 1; i <= len1; ++i) {
      for (int j = 1; j <= len2; ++j) {
          if (s1[i - 1] == s2[j - 1]) {
              dp[i][j] = dp[i - 1][j - 1];
          } else {
              dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
          }
      }
  }

  return dp[len1][len2];
}

void WordFinder::updateResults()
{
  if ( !searchInProgress.load() ) {
    return; // Old queued signal
  }

  if ( updateResultsTimer.isActive() ) {
    updateResultsTimer.stop(); // Can happen when we were done before it'd expire
  }

  std::u32string original = Folding::applySimpleCaseOnly( allWordWritings[ 0 ] );

  auto snapshot = queuedRequests.snapshot();

  bool all_finished = true;
  for ( const auto & request : snapshot ) {

    // Check if the request is finished
    if ( !request->isFinished() ) {
      all_finished = false;
      continue;
    }
    if ( !request->matchesCount() ) {
      continue;
    }
    // Set error message if the request has an error string
    if ( !request->getErrorString().isEmpty() ) {
      searchErrorString += tr( "Failed to query some dictionaries." );
      continue;
    }
    // Mark results as uncertain if the request is uncertain
    if ( request->isUncertain() && !searchResultsUncertain ) {
      searchResultsUncertain = true;
    }


    size_t count = request->matchesCount();

    for ( size_t x = 0; x < count; ++x ) {
      std::u32string match      = ( *request )[ x ].word;
      int weight                = ( *request )[ x ].weight;
      std::u32string lowerCased = Folding::applySimpleCaseOnly( match );

      if ( searchType == ExpressionMatch ) {
        unsigned ws;

        for ( ws = 0; ws < allWordWritings.size(); ws++ ) {
          if ( ws == 0 ) {
            // Check for prefix match with original expression
            if ( lowerCased.compare( 0, original.size(), original ) == 0 ) {
              break;
            }
          }
          else if ( lowerCased == Folding::applySimpleCaseOnly( allWordWritings[ ws ] ) ) {
            break;
          }
        }

        if ( ws >= allWordWritings.size() ) {
          // No exact matches found
          continue;
        }
        weight = ws;
      }

      auto insertResult =
        resultsIndex.insert( pair< std::u32string, ResultsArray::iterator >( lowerCased, resultsArray.end() ) );

      if ( !insertResult.second ) {
        // Wasn't inserted since there was already an item -- check the case
        if ( insertResult.first->second->word != match ) {
          // The case is different -- agree on a lowercase version
          insertResult.first->second->word = lowerCased;
        }
        if ( !weight && insertResult.first->second->wasSuggested ) {
          insertResult.first->second->wasSuggested = false;
        }
      }
      else {
        resultsArray.emplace_back();

        resultsArray.back().word         = match;
        resultsArray.back().rank         = INT_MAX;
        resultsArray.back().wasSuggested = ( weight != 0 );

        insertResult.first->second = --resultsArray.end();
      }
    }
  }

  size_t maxSearchResults = 500;

  if ( !resultsArray.empty() ) {
    if ( searchType == PrefixMatch ) {
      /// Assign each result a category, storing it in the rank's field

      for ( const auto & i : resultsIndex ) {
        i.second->rank = levenshteinDistance( allWordWriting[ 0 ], i.first );
      }

      resultsArray.sort( SortByRank() );
    }
    else if ( searchType == StemmedMatch ) {
      // Handling stemmed matches

      // We use two factors -- first is the number of characters strings share
      // in their beginnings, and second, the length of the strings. Here we assign
      // only the first one, storing it in rank. Then we sort the results using
      // SortByRankAndLength.
      for ( const auto & allWordWriting : allWordWritings ) {
        std::u32string target = Folding::apply( allWordWriting );

        for ( const auto & i : resultsIndex ) {
          std::u32string resultFolded = Folding::apply( i.first );

          int charsInCommon = 0;

          for ( char32_t const *t = target.c_str(), *r = resultFolded.c_str(); *t && *t == *r;
                ++t, ++r, ++charsInCommon ) {
            ;
          }

          int rank = -charsInCommon; // Negated so the lesser-than
                                     // comparison would yield right
                                     // results.

          if ( i.second->rank > rank ) {
            i.second->rank = rank; // We store the best rank of any writing
          }
        }
      }

      resultsArray.sort( SortByRankAndLength() );

      maxSearchResults = 15;
    }
  }

  searchResults.clear();
  searchResults.reserve( resultsArray.size() < maxSearchResults ? resultsArray.size() : maxSearchResults );

  for ( const auto & i : resultsArray ) {
    if ( searchResults.size() < maxSearchResults ) {
      searchResults.emplace_back( QString::fromStdU32String( i.word ), i.wasSuggested );
    }
    else {
      break;
    }
  }

  if ( !all_finished ) {
    // There are still some unhandled results.
    emit updated();
  }
  else {
    // That were all of them.
    searchInProgress = false;
    emit finished();
  }
}

void WordFinder::cancelSearches()
{
  auto snapshot = queuedRequests.snapshot();

  for ( const auto & queuedRequest : snapshot ) {
    if ( queuedRequest ) {
      queuedRequest->cancel();
    }
  }
}
