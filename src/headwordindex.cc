/* This file is part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

// xapian.h must be first to avoid macro collisions
#include "xapian.h"

#include "headwordindex.hh"
#include "folding.hh"
#include "utils.hh"

#include <QFile>
#include <QDir>
#include <QDebug>

namespace HeadwordIndex {

// Marker to identify a valid, completed index
static const std::string FINISH_MARKER = "hwindex_complete";

// Term prefix for exact headword matching
static const std::string TERM_PREFIX_EXACT = "XH";

// Term prefix for folded (lowercase) headword matching
static const std::string TERM_PREFIX_FOLDED = "XF";

//////////////////////////////////////////////////////////////////////////////
// HeadwordXapianIndex::Private
//////////////////////////////////////////////////////////////////////////////

class HeadwordXapianIndex::Private
{
public:
  std::unique_ptr< Xapian::Database > db;

  Private() = default;
};

//////////////////////////////////////////////////////////////////////////////
// HeadwordXapianIndex implementation
//////////////////////////////////////////////////////////////////////////////

HeadwordXapianIndex::HeadwordXapianIndex():
  d( std::make_unique< Private >() )
{
}

HeadwordXapianIndex::~HeadwordXapianIndex()
{
  close();
}

bool HeadwordXapianIndex::open( const std::string & path )
{
  QMutexLocker locker( &mutex );

  close();

  try {
    const QByteArray encodedPath = QFile::encodeName( QString::fromStdString( path ) );
    d->db                        = std::make_unique< Xapian::Database >( encodedPath.toStdString() );
    indexPath                    = path;
    return true;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to open headword index:" << e.get_description().c_str();
    return false;
  }
}

void HeadwordXapianIndex::close()
{
  QMutexLocker locker( &mutex );

  if ( d->db ) {
    d->db.reset();
  }
  indexPath.clear();
}

bool HeadwordXapianIndex::isOpen() const
{
  QMutexLocker locker( &mutex );
  return d->db != nullptr;
}

// Internal helper without mutex (assumes caller holds lock)
static int getTotalCountUnlocked( const std::unique_ptr< Xapian::Database > & db )
{
  if ( !db ) {
    return 0;
  }

  try {
    // Subtract 1 for the marker document
    return static_cast< int >( db->get_doccount() ) - 1;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to get document count:" << e.get_description().c_str();
    return 0;
  }
}

PagedResult HeadwordXapianIndex::getPage( int offset, int limit ) const
{
  PagedResult result;
  result.totalCount = 0;
  result.hasMore    = false;

  QMutexLocker locker( &mutex );

  if ( !d->db ) {
    return result;
  }

  try {
    result.totalCount = getTotalCountUnlocked( d->db );

    // Use Enquire with MatchAll to iterate in docid order
    Xapian::Enquire enquire( *d->db );
    enquire.set_query( Xapian::Query::MatchAll );
    enquire.set_weighting_scheme( Xapian::BoolWeight() );

    // Get results with offset
    Xapian::MSet mset = enquire.get_mset( offset, limit + 1 ); // +1 to check hasMore

    int count = 0;
    for ( Xapian::MSetIterator it = mset.begin(); it != mset.end() && count < limit; ++it, ++count ) {
      std::string data = it.get_document().get_data();
      if ( data != FINISH_MARKER ) {
        result.headwords.append( QString::fromUtf8( data.c_str() ) );
      }
    }

    result.hasMore = ( mset.size() > static_cast< Xapian::doccount >( limit ) );
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to get page:" << e.get_description().c_str();
  }

  return result;
}

PagedResult HeadwordXapianIndex::searchPrefix( const QString & prefix, int offset, int limit ) const
{
  PagedResult result;
  result.totalCount = 0;
  result.hasMore    = false;

  QMutexLocker locker( &mutex );

  if ( !d->db || prefix.isEmpty() ) {
    return result;
  }

  try {
    // Fold the prefix for case-insensitive matching
    std::u32string folded  = Folding::applySimpleCaseOnly( prefix.toStdU32String() );
    std::string foldedUtf8 = QString::fromStdU32String( folded ).toUtf8().toStdString();

    // Create prefix query using the folded term prefix
    Xapian::Query query( Xapian::Query::OP_WILDCARD,
                         TERM_PREFIX_FOLDED + foldedUtf8,
                         0, // max expansion (0 = unlimited)
                         Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT );

    Xapian::Enquire enquire( *d->db );
    enquire.set_query( query );

    Xapian::MSet mset = enquire.get_mset( offset, limit + 1 );

    result.totalCount = mset.get_matches_estimated();

    int count = 0;
    for ( Xapian::MSetIterator it = mset.begin(); it != mset.end() && count < limit; ++it, ++count ) {
      std::string data = it.get_document().get_data();
      if ( data != FINISH_MARKER ) {
        result.headwords.append( QString::fromUtf8( data.c_str() ) );
      }
    }

    result.hasMore = ( mset.size() > static_cast< Xapian::doccount >( limit ) );
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to search prefix:" << e.get_description().c_str();
  }

  return result;
}

PagedResult HeadwordXapianIndex::searchWildcard( const QString & pattern, int offset, int limit ) const
{
  PagedResult result;
  result.totalCount = 0;
  result.hasMore    = false;

  QMutexLocker locker( &mutex );

  if ( !d->db || pattern.isEmpty() ) {
    return result;
  }

  try {
    Xapian::QueryParser qp;
    qp.set_database( *d->db );
    qp.add_prefix( "", TERM_PREFIX_FOLDED );

    // Fold the pattern
    std::u32string folded   = Folding::applySimpleCaseOnly( pattern.toStdU32String() );
    std::string patternUtf8 = QString::fromStdU32String( folded ).toUtf8().toStdString();

    int flags = Xapian::QueryParser::FLAG_WILDCARD | Xapian::QueryParser::FLAG_CJK_NGRAM;

    Xapian::Query query = qp.parse_query( patternUtf8, flags );

    qDebug() << "Headword wildcard query:" << query.get_description().c_str();

    Xapian::Enquire enquire( *d->db );
    enquire.set_query( query );

    Xapian::MSet mset = enquire.get_mset( offset, limit + 1 );

    result.totalCount = mset.get_matches_estimated();

    int count = 0;
    for ( Xapian::MSetIterator it = mset.begin(); it != mset.end() && count < limit; ++it, ++count ) {
      std::string data = it.get_document().get_data();
      if ( data != FINISH_MARKER ) {
        result.headwords.append( QString::fromUtf8( data.c_str() ) );
      }
    }

    result.hasMore = ( mset.size() > static_cast< Xapian::doccount >( limit ) );
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to search wildcard:" << e.get_description().c_str();
  }

  return result;
}

QStringList HeadwordXapianIndex::suggestSpelling( const QString & term, int maxSuggestions ) const
{
  QStringList suggestions;

  QMutexLocker locker( &mutex );

  if ( !d->db || term.isEmpty() ) {
    return suggestions;
  }

  try {
    std::u32string folded  = Folding::applySimpleCaseOnly( term.toStdU32String() );
    std::string foldedUtf8 = QString::fromStdU32String( folded ).toUtf8().toStdString();

    // Get spelling suggestions
    Xapian::TermIterator it  = d->db->spellings_begin();
    Xapian::TermIterator end = d->db->spellings_end();

    // Simple approach: find terms that start similarly
    // For better fuzzy matching, we'd need edit distance calculation
    for ( ; it != end && suggestions.size() < maxSuggestions; ++it ) {
      std::string suggestion = *it;
      // Basic similarity check - shares common prefix
      if ( suggestion.substr( 0, std::min( suggestion.size(), foldedUtf8.size() / 2 + 1 ) )
           == foldedUtf8.substr( 0, std::min( suggestion.size(), foldedUtf8.size() / 2 + 1 ) ) ) {
        suggestions.append( QString::fromUtf8( suggestion.c_str() ) );
      }
    }
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to get spelling suggestions:" << e.get_description().c_str();
  }

  return suggestions;
}

//////////////////////////////////////////////////////////////////////////////
// HeadwordIndexBuilder::Private
//////////////////////////////////////////////////////////////////////////////

class HeadwordIndexBuilder::Private
{
public:
  std::unique_ptr< Xapian::WritableDatabase > db;
  Xapian::TermGenerator indexer;

  Private() = default;
};

//////////////////////////////////////////////////////////////////////////////
// HeadwordIndexBuilder implementation
//////////////////////////////////////////////////////////////////////////////

HeadwordIndexBuilder::HeadwordIndexBuilder():
  d( std::make_unique< Private >() ),
  indexedCount( 0 )
{
}

HeadwordIndexBuilder::~HeadwordIndexBuilder()
{
  cancel();
}

bool HeadwordIndexBuilder::start( const std::string & path )
{
  try {
    indexPath    = path + "_temp";
    indexedCount = 0;

    const QByteArray encodedPath = QFile::encodeName( QString::fromStdString( indexPath ) );
    d->db = std::make_unique< Xapian::WritableDatabase >( encodedPath.toStdString(), Xapian::DB_CREATE_OR_OVERWRITE );

    d->indexer.set_flags( Xapian::TermGenerator::FLAG_CJK_NGRAM );

    return true;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to start headword index:" << e.get_description().c_str();
    return false;
  }
}

void HeadwordIndexBuilder::addHeadword( const QString & headword )
{
  if ( !d->db ) {
    return;
  }

  try {
    Xapian::Document doc;

    // Store original headword as document data
    doc.set_data( headword.toUtf8().toStdString() );

    // Add exact term (original case)
    std::string headwordUtf8 = headword.toUtf8().toStdString();
    doc.add_term( TERM_PREFIX_EXACT + headwordUtf8 );

    // Add folded term for case-insensitive search
    std::u32string folded  = Folding::applySimpleCaseOnly( headword.toStdU32String() );
    std::string foldedUtf8 = QString::fromStdU32String( folded ).toUtf8().toStdString();
    doc.add_term( TERM_PREFIX_FOLDED + foldedUtf8 );

    // Index the headword text for full-text search within headwords
    d->indexer.set_document( doc );
    d->indexer.index_text( headwordUtf8 );

    // Add to spelling dictionary for fuzzy suggestions
    d->db->add_spelling( foldedUtf8 );

    d->db->add_document( doc );
    indexedCount++;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to add headword:" << e.get_description().c_str();
  }
}

bool HeadwordIndexBuilder::finish()
{
  if ( !d->db ) {
    return false;
  }

  try {
    // Add marker document to indicate completion
    Xapian::Document markerDoc;
    markerDoc.set_data( FINISH_MARKER );
    d->db->add_document( markerDoc );

    d->db->commit();

    // Compact to final location
    std::string finalPath = indexPath;
    // Remove "_temp" suffix
    if ( finalPath.size() > 5 && finalPath.substr( finalPath.size() - 5 ) == "_temp" ) {
      finalPath = finalPath.substr( 0, finalPath.size() - 5 );
    }

    const QByteArray encodedFinalPath = QFile::encodeName( QString::fromStdString( finalPath ) );
    d->db->compact( encodedFinalPath.toStdString() );

    d->db->close();
    d->db.reset();

    // Remove temp directory
    Utils::Fs::removeDirectory( indexPath );

    return true;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Failed to finish headword index:" << e.get_description().c_str();
    cancel();
    return false;
  }
}

void HeadwordIndexBuilder::cancel()
{
  if ( d->db ) {
    try {
      d->db->close();
    }
    catch ( ... ) {
    }
    d->db.reset();
  }

  if ( !indexPath.empty() ) {
    Utils::Fs::removeDirectory( indexPath );
    indexPath.clear();
  }

  indexedCount = 0;
}

int HeadwordIndexBuilder::getIndexedCount() const
{
  return indexedCount;
}

//////////////////////////////////////////////////////////////////////////////
// Helper functions
//////////////////////////////////////////////////////////////////////////////

bool indexIsValid( const std::string & indexPath )
{
  try {
    const QByteArray encodedPath = QFile::encodeName( QString::fromStdString( indexPath ) );
    Xapian::Database db( encodedPath.toStdString() );

    auto docid    = db.get_lastdocid();
    auto document = db.get_document( docid );

    return document.get_data() == FINISH_MARKER;
  }
  catch ( const Xapian::Error & e ) {
    qWarning() << "Headword index validation failed:" << e.get_description().c_str();
    return false;
  }
  catch ( ... ) {
    return false;
  }
}

} // namespace HeadwordIndex
