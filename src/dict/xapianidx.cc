/* This file is (c) 2024 xiaoyifang, based on original work by Konstantin Isakov
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "xapian.h"
#include "xapianidx.hh"
#include "utils.hh"
#include <QDebug>
#include <QFile>

namespace XapianIndexing {

// Helper function to generate Xapian index file path from dictionary ID
std::string getXapianIndexFilePath( const std::string & dictId ) {
    return Config::getIndexDir().toStdString() + dictId + ".xapianidx";
}

StreamedXapianIndexer::StreamedXapianIndexer(const std::string& dbPath)
    : db(dbPath, Xapian::DB_CREATE_OR_OPEN)
{
    // 使用 CJK N-gram 分词器以支持中日韩语言的全文搜索
    indexer.set_flags(Xapian::TermGenerator::FLAG_CJK_NGRAM);
}

StreamedXapianIndexer::~StreamedXapianIndexer()
{
    if (!finished) {
        // 确保在析构时，即使没有显式调用 finish()，数据库也能被正确关闭
        // 尽管这通常表示一个未完成的索引过程
        try {
            db.close();
        } catch (const Xapian::Error& e) {
            qWarning("Xapian error on closing database in destructor: %s", e.get_msg().c_str());
        }
    }
}

void StreamedXapianIndexer::addWord(const QString& word, uint32_t offset)
{
    Xapian::Document doc;
    indexer.set_document(doc);
    indexer.index_text(word.toStdString());
    doc.set_data(std::to_string(offset));
    db.add_document(doc);
}

/// Build Xapian index using dictionary ID
/// @param indexedWords Map of headwords and their corresponding article link counts
/// @param dictId Dictionary ID
void buildXapianIndex( std::map< QString, uint32_t > const & indexedWords, const std::string & dictId ) {
  try {
    std::string file = getXapianIndexFilePath( dictId );
    
    // Open the database for update, creating a new database if necessary.
    Xapian::WritableDatabase db( file + "_temp", Xapian::DB_CREATE_OR_OPEN );

    Xapian::TermGenerator indexer;
    //  Xapian::Stem stemmer("english");
    //  indexer.set_stemmer(stemmer);
    //  indexer.set_stemming_strategy(indexer.STEM_SOME_FULL_POS);
    indexer.set_flags( Xapian::TermGenerator::FLAG_CJK_NGRAM );

    for ( const auto &[ word, articleLinks ] : indexedWords ) {
      Xapian::Document doc;
      indexer.set_document( doc );
      indexer.index_text( word.toStdString() );
      doc.set_data( std::to_string( articleLinks ) );
      doc.add_value(0,word.toStdString());
      // Add the document to the database.
      db.add_document( doc );
    }
    db.commit();

    db.compact( file );

    db.close();

    Utils::Fs::removeDirectory( file + "_temp" );
  } catch ( Xapian::Error & e ) {
    qWarning() << "create xapian headword index:" << QString::fromStdString( e.get_description() );
  }
}

/// Batch read all indexed headwords using dictionary ID
/// @param dictId Dictionary ID
/// @return Map containing all headwords and their corresponding article link counts
std::map< QString, uint32_t > getAllIndexedWords( const std::string & dictId ) {
  std::map< QString, uint32_t > indexedWords;
  std::string file = getXapianIndexFilePath( dictId );
  
  try {
    // Open the Xapian database for read-only access
    Xapian::Database db( file );
    
    // Get the total number of documents in the database
    Xapian::doccount docCount = db.get_doccount();
    
    // Iterate through all documents
    for ( Xapian::doccount i = 1; i <= docCount; ++i ) {
      try {
        Xapian::Document doc = db.get_document( i );
        
        auto term = doc.get_value(0);
        
        QString word = QString::fromStdString( term );
          
        // Get the article link count from document data
        std::string data = doc.get_data();
        uint32_t articleLinks = std::stoi( data );
        
        indexedWords[ word ] = articleLinks;
      } catch ( const Xapian::DocNotFoundError & ) {
        // Document not found, skip
        continue;
      }
    }
  } catch ( Xapian::Error & e ) {
    qWarning() << "read xapian headword index:" << QString::fromStdString( e.get_description() );
  }
  
  return indexedWords;
void StreamedXapianIndexer::addWord(const QString& word, uint32_t offset)
{
    Xapian::Document doc;
    indexer.set_document(doc);
    indexer.index_text(word.toStdString());
    doc.set_data(std::to_string(offset));
    db.add_document(doc);
}

/// Batch read indexed headwords with pagination using dictionary ID
/// @param dictId Dictionary ID
/// @param offset Starting position (0-based)
/// @param pageSize Number of items per page
/// @param totalCount Output parameter to get the total number of indexed words
/// @return Map containing headwords and their corresponding article link counts for the specified range
std::map< QString, uint32_t > getIndexedWordsByOffset( const std::string & dictId, uint32_t offset, uint32_t pageSize, uint32_t & totalCount ) {
  std::map< QString, uint32_t > indexedWords;
  totalCount = 0;
  std::string file = getXapianIndexFilePath( dictId );
  
  try {
    // Open the Xapian database for read-only access
    Xapian::Database db( file );
    
    // Get the total number of documents in the database
    Xapian::doccount docCount = db.get_doccount();
    totalCount = static_cast< uint32_t >( docCount );
    
    // Validate offset
    if ( offset >= docCount ) {
      // Return empty map for invalid offset
      return indexedWords;
    }
    
    // Create an enquire object for database operations
    Xapian::Enquire enquire( db );
    
    // Create a query that matches all documents (Xapian::Query::MatchAll)
    Xapian::Query query = Xapian::Query( Xapian::Query::OP_TRUE );
    enquire.set_query( query );
    
    // Use Xapian's internal pagination to get the desired range of results
    // This is much more efficient than manually iterating through documents
    Xapian::MSet matches = enquire.get_mset( offset, pageSize );
    
    // Process the matched documents
    for ( Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i ) {
      try {
        Xapian::Document doc = i.get_document();
        
        auto term = doc.get_value(0);
        
        QString word = QString::fromStdString( term );
          
        // Get the article link count from document data
        std::string data = doc.get_data();
        uint32_t articleLinks = std::stoi( data );
        
        indexedWords[ word ] = articleLinks;
      } catch ( const Xapian::DocNotFoundError & ) {
        // Document not found, skip
        continue;
      }
    }
  } catch ( Xapian::Error & e ) {
    qWarning() << "read xapian headword index with pagination:" << QString::fromStdString( e.get_description() );
  }
  
  return indexedWords;
void StreamedXapianIndexer::finish()
{
    db.commit();
    finished = true;
}

/// Search indexed headwords with pagination using dictionary ID
/// @param dictId Dictionary ID
/// @param searchQuery Search query string
/// @param offset Starting position (0-based)
/// @param pageSize Number of items per page
/// @param totalCount Output parameter to get the total number of matched words
/// @return Map containing matched headwords and their corresponding article link counts for the specified range
std::map< QString, uint32_t > searchIndexedWordsByOffset( const std::string & dictId, const std::string & searchQuery, uint32_t offset, uint32_t pageSize, uint32_t & totalCount ) {
  std::map< QString, uint32_t > indexedWords;
  totalCount = 0;
  std::string file = getXapianIndexFilePath( dictId );
  
  try {
    // Open the Xapian database for read-only access
    Xapian::Database db( file );
    
    // Create an enquire object for database operations
    Xapian::Enquire enquire( db );
    
    // Create a query parser for better search handling
    Xapian::QueryParser parser;
    parser.set_database( db );
    parser.set_stemming_strategy( Xapian::QueryParser::STEM_NONE );
    
    // Set flags for better CJK (Chinese, Japanese, Korean) support
    // This should match the flags used during indexing
    parser.set_default_op( Xapian::Query::OP_AND );
    
    // Parse the search query
    Xapian::Query query = parser.parse_query( searchQuery, Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_CJK_NGRAM | Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_PARTIAL );
    
    // Set query to enquire object
    enquire.set_query( query );
    
    // Use Xapian's internal pagination to get the desired range of search results
    Xapian::MSet matches = enquire.get_mset( offset, pageSize );
    
    // Get the total number of matches
    totalCount = static_cast< uint32_t >( matches.get_matches_estimated() );
    
    // Process the matched documents
    for ( Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i ) {
      try {
        Xapian::Document doc = i.get_document();
        
        auto term = doc.get_value(0);
        
        QString word = QString::fromStdString( term );
          
        // Get the article link count from document data
        std::string data = doc.get_data();
        uint32_t articleLinks = std::stoi( data );
        
        indexedWords[ word ] = articleLinks;
      } catch ( const Xapian::DocNotFoundError & ) {
        // Document not found, skip
        continue;
      }
    }
  } catch ( Xapian::Error & e ) {
    qWarning() << "search xapian headword index with pagination:" << QString::fromStdString( e.get_description() );
  }
  
  return indexedWords;
}

} // namespace XapianIndexing