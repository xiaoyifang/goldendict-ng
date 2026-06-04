/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "wordfinder.hh"
#include "folding.hh"
#include <map>


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

void WordFinder::prefixMatch( const QString & str,
                              const std::vector< sptr< Dictionary::Class > > & dicts,
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
void WordFinder::stemmedMatch( const QString & str,
                               const std::vector< sptr< Dictionary::Class > > & dicts,
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

void WordFinder::expressionMatch( const QString & str,
                                  const std::vector< sptr< Dictionary::Class > > & dicts,
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
  std::vector< std::u32string > allWordWritings( 1, inputWord.toStdU32String() );

  for ( const auto & inputDict : *inputDicts ) {
    vector< std::u32string > writings = inputDict->getAlternateWritings( allWordWritings[ 0 ] );

    allWordWritings.insert( allWordWritings.end(), writings.begin(), writings.end() );
  }

  setAllWordWritings( allWordWritings );
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

enum Score {
  ScoreExactMatch              = 100,
  ScoreExactNoFullCaseMatch    = 95,
  ScoreExactNoDiaMatch         = 90,
  ScoreExactNoPunctMatch       = 85,
  ScoreExactNoWsMatch          = 80,
  ScoreExactInsideMatch        = 75,
  ScoreExactNoDiaInsideMatch   = 70,
  ScoreExactNoPunctInsideMatch = 65,
  ScorePrefixMatch             = 60,
  ScorePrefixNoDiaMatch        = 55,
  ScorePrefixNoPunctMatch      = 50,
  ScorePrefixNoWsMatch         = 45,
  ScoreWorstMatch              = 0
};

unsigned saturated( unsigned x )
{
  return x < 255 ? x : 255;
}

/// Checks whether the first string has the second one inside, surrounded from
/// both sides by either whitespace, punctuation or begin/end of string.
/// If true is returned, pos holds the offset in the haystack. If the offset
/// is larger than 255, it is set to 255.
bool hasSurroundedWithWs( const std::u32string & haystack,
                          const std::u32string & needle,
                          std::u32string::size_type & pos )
{
  if ( haystack.size() < needle.size() ) {
    return false;
  }

  for ( pos = 0;; ++pos ) {
    pos = haystack.find( needle, pos );
    if ( pos == std::u32string::npos ) {
      return false;
    }

    if ( ( !pos || Folding::isWhitespace( haystack[ pos - 1 ] ) || Folding::isPunct( haystack[ pos - 1 ] ) )
         && ( ( pos + needle.size() == haystack.size() ) || Folding::isWhitespace( haystack[ pos + needle.size() ] )
              || Folding::isPunct( haystack[ pos + needle.size() ] ) ) ) {
      pos = saturated( pos );
      return true;
    }
  }
}

struct ResultFoldings
{
  std::u32string noFullCase;
  std::u32string noDia;
  std::u32string noPunct;
  std::u32string noWs;
};

ResultFoldings computeFoldings( const std::u32string & str )
{
  ResultFoldings f;
  f.noFullCase = Folding::applyFullCaseOnly( str );
  f.noDia      = Folding::applyDiacriticsOnly( f.noFullCase );
  f.noPunct    = Folding::applyPunctOnly( f.noDia );
  f.noWs       = Folding::applyWhitespaceOnly( f.noPunct );
  return f;
}

int computeMatchScore( const std::u32string & result,
                       const std::u32string & target,
                       const std::u32string & targetNoFullCase,
                       const std::u32string & targetNoDia,
                       const std::u32string & targetNoPunct,
                       const std::u32string & targetNoWs,
                       std::u32string::size_type & matchPos,
                       const ResultFoldings & rf )
{
  if ( result == target ) {
    return ScoreExactMatch;
  }
  if ( rf.noFullCase == targetNoFullCase ) {
    return ScoreExactNoFullCaseMatch;
  }
  if ( rf.noDia == targetNoDia ) {
    return ScoreExactNoDiaMatch;
  }
  if ( rf.noPunct == targetNoPunct ) {
    return ScoreExactNoPunctMatch;
  }
  if ( rf.noWs == targetNoWs ) {
    return ScoreExactNoWsMatch;
  }
  if ( hasSurroundedWithWs( result, target, matchPos ) ) {
    return ScoreExactInsideMatch;
  }
  if ( hasSurroundedWithWs( rf.noDia, targetNoDia, matchPos ) ) {
    return ScoreExactNoDiaInsideMatch;
  }
  if ( hasSurroundedWithWs( rf.noPunct, targetNoPunct, matchPos ) ) {
    return ScoreExactNoPunctInsideMatch;
  }
  if ( result.size() > target.size() && result.compare( 0, target.size(), target ) == 0 ) {
    return ScorePrefixMatch;
  }
  if ( rf.noDia.size() > targetNoDia.size() && rf.noDia.compare( 0, targetNoDia.size(), targetNoDia ) == 0 ) {
    return ScorePrefixNoDiaMatch;
  }
  if ( rf.noPunct.size() > targetNoPunct.size() && rf.noPunct.compare( 0, targetNoPunct.size(), targetNoPunct ) == 0 ) {
    return ScorePrefixNoPunctMatch;
  }
  if ( rf.noWs.size() > targetNoWs.size() && rf.noWs.compare( 0, targetNoWs.size(), targetNoWs ) == 0 ) {
    return ScorePrefixNoWsMatch;
  }
  return ScoreWorstMatch;
}

} // namespace

void WordFinder::updateResults()
{
  if ( !searchInProgress.load() ) {
    return; // Old queued signal
  }

  if ( updateResultsTimer.isActive() ) {
    updateResultsTimer.stop(); // Can happen when we were done before it'd expire
  }

  auto allWordWritings = getAllWordWritings();

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
        resultsArray.back().rankFeatures = RankFeatures( 0, INT_MAX );

        insertResult.first->second = --resultsArray.end();
      }
    }
  }

  size_t maxSearchResults = 500;

  if ( !resultsArray.empty() ) {
    auto computePrefixScore = []( const std::u32string & result,
                                  const std::u32string & target,
                                  const std::u32string & targetNoFullCase,
                                  const std::u32string & targetNoDia,
                                  const std::u32string & targetNoPunct,
                                  const std::u32string & targetNoWs,
                                  const ResultFoldings & rf ) -> RankFeatures {
      std::u32string::size_type matchPos = 0;
      int score =
        computeMatchScore( result, target, targetNoFullCase, targetNoDia, targetNoPunct, targetNoWs, matchPos, rf );
      int lengthDelta = std::abs( static_cast< int >( target.size() ) - static_cast< int >( result.size() ) );
      return RankFeatures( score, lengthDelta );
    };

    auto computeStemmedScore = []( const std::u32string & result, const std::u32string & target ) -> RankFeatures {
      std::u32string resultFolded = Folding::apply( result );
      std::u32string targetFolded = Folding::apply( target );

      int charsInCommon = 0;
      for ( const char32_t *t = targetFolded.c_str(), *r = resultFolded.c_str(); *t && *t == *r;
            ++t, ++r, ++charsInCommon ) {
        ;
      }
      int lengthDelta = std::abs( static_cast< int >( target.size() ) - static_cast< int >( result.size() ) );
      return RankFeatures( charsInCommon, lengthDelta );
    };

    if ( searchType == PrefixMatch ) {
      std::map< std::u32string, ResultFoldings > resultFoldingsCache;
      for ( const auto & i : resultsIndex ) {
        resultFoldingsCache[ i.first ] = computeFoldings( i.first );
      }

      for ( const auto & allWordWriting : allWordWritings ) {
        std::u32string target           = Folding::applySimpleCaseOnly( allWordWriting );
        std::u32string targetNoFullCase = Folding::applyFullCaseOnly( target );
        std::u32string targetNoDia      = Folding::applyDiacriticsOnly( targetNoFullCase );
        std::u32string targetNoPunct    = Folding::applyPunctOnly( targetNoDia );
        std::u32string targetNoWs       = Folding::applyWhitespaceOnly( targetNoPunct );

        for ( const auto & i : resultsIndex ) {
          RankFeatures rf = computePrefixScore( i.first,
                                                target,
                                                targetNoFullCase,
                                                targetNoDia,
                                                targetNoPunct,
                                                targetNoWs,
                                                resultFoldingsCache[ i.first ] );

          RankFeatures & destRf = i.second->rankFeatures;
          if ( rf.baseScore > destRf.baseScore
               || ( rf.baseScore == destRf.baseScore && rf.lengthDelta < destRf.lengthDelta ) ) {
            destRf = rf;
          }
        }
      }
      resultsArray.sort( SortByRankFeatures() );
    }
    else if ( searchType == StemmedMatch ) {
      for ( const auto & allWordWriting : allWordWritings ) {
        for ( const auto & i : resultsIndex ) {
          RankFeatures rf = computeStemmedScore( i.first, allWordWriting );

          RankFeatures & destRf = i.second->rankFeatures;
          if ( rf.baseScore > destRf.baseScore
               || ( rf.baseScore == destRf.baseScore && rf.lengthDelta < destRf.lengthDelta ) ) {
            destRf = rf;
          }
        }
      }
      resultsArray.sort( SortByRankFeatures() );
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
