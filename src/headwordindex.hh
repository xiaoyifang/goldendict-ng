/* This file is part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QString>
#include <QStringList>
#include <QMutex>
#include <cstdint>
#include <memory>
#include <string>

namespace HeadwordIndex {

/// Result structure for paginated headword queries
struct PagedResult
{
  QStringList headwords;
  int totalCount; // Total matching count (estimated for searches)
  bool hasMore;   // Whether there are more results available
};

/// A Xapian-based index for efficient paginated headword retrieval and search.
/// This index is separate from the FTS index and optimized for:
/// - Fast paginated browsing (offset-based pagination)
/// - Prefix search with pagination
/// - Fuzzy search via spelling suggestions
class HeadwordXapianIndex
{
public:
  HeadwordXapianIndex();
  ~HeadwordXapianIndex();

  /// Open an existing index for reading
  /// @param indexPath Path to the Xapian database directory
  /// @return true if opened successfully
  bool open( const std::string & indexPath );

  /// Close the index
  void close();

  /// Check if index is open and valid
  bool isOpen() const;

  /// Get total headword count in the index
  int getTotalCount() const;

  /// Get a page of headwords (sorted by document ID, which preserves insertion order)
  /// @param offset Starting position (0-based)
  /// @param limit Maximum number of results to return
  /// @return PagedResult containing headwords and metadata
  PagedResult getPage( int offset, int limit ) const;

  /// Search headwords with prefix matching
  /// @param prefix The prefix to search for (will be folded to lowercase)
  /// @param offset Starting position within search results
  /// @param limit Maximum number of results
  /// @return PagedResult containing matching headwords
  PagedResult searchPrefix( const QString & prefix, int offset, int limit ) const;

  /// Search headwords with wildcard pattern (e.g., "test*")
  /// @param pattern Wildcard pattern (supports * at end for prefix matching)
  /// @param offset Starting position within search results
  /// @param limit Maximum number of results
  /// @return PagedResult containing matching headwords
  PagedResult searchWildcard( const QString & pattern, int offset, int limit ) const;

  /// Get spelling suggestions for a term (fuzzy search)
  /// @param term The term to get suggestions for
  /// @param maxSuggestions Maximum number of suggestions
  /// @return List of suggested terms
  QStringList suggestSpelling( const QString & term, int maxSuggestions = 5 ) const;

  /// Get the index file path
  const std::string & getIndexPath() const
  {
    return indexPath;
  }

private:
  class Private;
  std::unique_ptr< Private > d;
  std::string indexPath;
  mutable QMutex mutex;
};

/// Builder class for creating headword indexes
class HeadwordIndexBuilder
{
public:
  HeadwordIndexBuilder();
  ~HeadwordIndexBuilder();

  /// Start building a new index
  /// @param indexPath Path where the index will be created
  /// @return true if initialization successful
  bool start( const std::string & indexPath );

  /// Add a headword to the index
  /// @param headword The headword to add
  void addHeadword( const QString & headword );

  /// Finish building and compact the index
  /// @return true if finalization successful
  bool finish();

  /// Cancel building and clean up
  void cancel();

  /// Get current number of indexed headwords
  int getIndexedCount() const;

private:
  class Private;
  std::unique_ptr< Private > d;
  std::string indexPath;
  int indexedCount;
};

/// Check if headword index exists and is valid
bool indexIsValid( const std::string & indexPath );

} // namespace HeadwordIndex
