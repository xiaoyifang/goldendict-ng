/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dict/dictionary.hh"
#include <map>
#include <string>
#include <vector>

namespace XapianIndexing {

// Helper function to generate Xapian index file path from dictionary ID
std::string getXapianIndexFilePath( const std::string & dictId );

/// Build Xapian index using dictionary ID
/// @param indexedWords Map of headwords and their corresponding article link counts
/// @param dictId Dictionary ID
void buildXapianIndex( std::map< QString, uint32_t > const & indexedWords, const std::string & dictId );

/// Batch read all indexed headwords using dictionary ID
/// @param dictId Dictionary ID
/// @return Map containing all headwords and their corresponding article link counts
std::map< QString, uint32_t > getAllIndexedWords( const std::string & dictId );

/// Batch read indexed headwords with pagination using dictionary ID
/// @param dictId Dictionary ID
/// @param offset Starting position (0-based)
/// @param pageSize Number of items per page
/// @param totalCount Output parameter to get the total number of indexed words
/// @return Map containing headwords and their corresponding article link counts for the specified range
std::map< QString, uint32_t > getIndexedWordsByOffset( const std::string & dictId, uint32_t offset, uint32_t pageSize, uint32_t & totalCount );

/// Search indexed headwords with pagination using dictionary ID
/// @param dictId Dictionary ID
/// @param searchQuery Search query string
/// @param offset Starting position (0-based)
/// @param pageSize Number of items per page
/// @param totalCount Output parameter to get the total number of matched words
/// @return Map containing matched headwords and their corresponding article link counts for the specified range
std::map< QString, uint32_t > searchIndexedWordsByOffset( const std::string & dictId, const std::string & searchQuery, uint32_t offset, uint32_t pageSize, uint32_t & totalCount );

} // namespace XapianIndexing