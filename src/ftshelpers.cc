/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#include "xapian.h"
#include <cstdlib>
#include "fulltextsearch.hh"
#include "ftshelpers.hh"
#include "wstring_qt.hh"
#include "file.hh"
#include "gddebug.hh"
#include "folding.hh"
#include "utils.hh"

#include <vector>
#include <string>

#include <QVector>

#include <QRegularExpression>

#include "wildcard.hh"
#include "globalregex.hh"
#include <QSemaphoreReleaser>

using std::vector;
using std::string;

DEF_EX( exUserAbort, "User abort", Dictionary::Ex )

namespace FtsHelpers
{
// finished  reversed   dehsinif
const static std::string finish_mark = std::string( "dehsinif" );

bool ftsIndexIsOldOrBad( BtreeIndexing::BtreeDictionary * dict )
{
  try {
    Xapian::WritableDatabase db( dict->ftsIndexName() );
    auto docid    = db.get_lastdocid();
    auto document = db.get_document( docid );

    qDebug() << document.get_data().c_str();
    //use a special document to mark the end of the index.
    return document.get_data() != finish_mark;
  }
  catch( Xapian::Error & e )
  {
    qWarning() << e.get_description().c_str();
    //the file is corrupted,remove it.
    QFile::remove(QString::fromStdString(dict->ftsIndexName()));
    return true;
  }
  catch( ... )
  {
    return true;
  }
}

static QString makeHiliteRegExpString( QStringList const & words,
                                       int searchMode, int distanceBetweenWords, bool hasCJK = false, bool ignoreWordsOrder = false )
{
  QString searchString( "(" );

  QString stripWords( "(?:\\W+\\w+){0," );

  if( hasCJK )
  {
    stripWords = "(?:[\\W\\w]){0,";
  }

  if( distanceBetweenWords >= 0 )
    stripWords += QString::number( distanceBetweenWords );
  stripWords += "}";

  if(!hasCJK)
  {
    stripWords += "\\W+";
  }

  QString boundWord( searchMode == FTS::WholeWords ? "\\b" : "(?:\\w*)");
  if(hasCJK)
  {
    //no boundary for CJK
    boundWord.clear();
  }

  for( int x = 0; x < words.size(); x++ )
  {
    if( x )
    {
      searchString += stripWords;
      if(ignoreWordsOrder)
        searchString += "(";
    }

    searchString += boundWord + words[ x ] + boundWord;

    if( x )
    {
      if( ignoreWordsOrder )
        searchString += ")?";
    }

  }

  searchString += ")";
  return searchString;
}

void tokenizeCJK( QStringList & indexWords, QRegularExpression wordRegExp, QStringList list )
{
  QStringList wordList, hieroglyphList;
  for(auto word : list)
  {
    // Check for CJK symbols in word
    bool parsed = false;
    QString hieroglyph;
    for( int x = 0; x < word.size(); x++ )
      if ( Utils::isCJKChar( word.at( x ).unicode() ) ) {
        parsed = true;
        hieroglyph.append( word[ x ] );

        if( QChar( word.at( x ) ).isHighSurrogate()
            &&  QChar( word[ x + 1 ] ).isLowSurrogate() )
          hieroglyph.append( word[ ++x ] );

        hieroglyphList.append( hieroglyph );
        hieroglyph.clear();
      }

    // If word don't contains CJK symbols put it in list as is
    if( !parsed )
      wordList.append( word );
  }

  indexWords = wordList.filter( wordRegExp );
  indexWords.removeDuplicates();

  hieroglyphList.removeDuplicates();
  indexWords += hieroglyphList;
}

bool containCJK( QString const & str)
{
  bool hasCJK = false;
  for(auto x : str)
    if ( Utils::isCJKChar( x.unicode() ) ) {
      hasCJK = true;
      break;
    }
  return hasCJK;
}

void makeFTSIndex( BtreeIndexing::BtreeDictionary * dict, QAtomicInt & isCancelled )
{
  QMutexLocker _( &dict->getFtsMutex() );

  //check the index again.
  if ( dict->haveFTSIndex() )
    return;

  try {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      throw exUserAbort();

    // Open the database for update, creating a new database if necessary.
    Xapian::WritableDatabase db( dict->ftsIndexName(), Xapian::DB_CREATE_OR_OPEN );

    Xapian::TermGenerator indexer;
    //  Xapian::Stem stemmer("english");
    //  indexer.set_stemmer(stemmer);
    //  indexer.set_stemming_strategy(indexer.STEM_SOME_FULL_POS);
    indexer.set_flags( Xapian::TermGenerator::FLAG_CJK_NGRAM );

    BtreeIndexing::IndexedWords indexedWords;

    QSet< uint32_t > setOfOffsets;
    setOfOffsets.reserve( dict->getArticleCount() );

    dict->findArticleLinks( nullptr, &setOfOffsets, nullptr, &isCancelled );

    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      throw exUserAbort();

    QVector< uint32_t > offsets;
    offsets.resize( setOfOffsets.size() );
    uint32_t * ptr = &offsets.front();

    for ( QSet< uint32_t >::ConstIterator it = setOfOffsets.constBegin(); it != setOfOffsets.constEnd(); ++it ) {
      *ptr = *it;
      ptr++;
    }

    // Free memory
    setOfOffsets.clear();

    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      throw exUserAbort();

    dict->sortArticlesOffsetsForFTS( offsets, isCancelled );

    // incremental build the index.
    // get the last address.
    bool skip            = true;
    uint32_t lastAddress = -1;
    try {
      if ( db.get_lastdocid() > 0 ) {
        Xapian::Document lastDoc = db.get_document( db.get_lastdocid() );
        lastAddress              = atoi( lastDoc.get_data().c_str() );
      }
      else {
        skip = false;
      }
    }
    catch ( Xapian::Error & e ) {
      qDebug() << "get last doc failed: " << e.get_description().c_str();
      skip = false;
    }

    long indexedDoc = 0L;

    for ( auto const & address : offsets ) {
      indexedDoc++;

      if ( address > lastAddress && skip ) {
        skip = false;
      }
      //skip until to the lastAddress;
      if ( skip ) {
        continue;
      }

      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        return;
      }

      QString headword, articleStr;

      dict->getArticleText( address, headword, articleStr );

      Xapian::Document doc;

      indexer.set_document( doc );

      if ( GlobalBroadcaster::instance()->getPreference()->fts.enablePosition ) {
        indexer.index_text( articleStr.toStdString() );
      }
      else {
        indexer.index_text_without_positions( articleStr.toStdString() );
      }

      doc.set_data( std::to_string( address ) );
      // Add the document to the database.
      db.add_document( doc );
      dict->setIndexedFtsDoc( indexedDoc );
    }

    //add a special document to mark the end of the index.
    Xapian::Document doc;
    doc.set_data( finish_mark );
    // Add the document to the database.
    db.add_document( doc );

    // Free memory
    offsets.clear();

    db.commit();
  }
  catch ( Xapian::Error & e ) {
    qWarning() << "create xapian index:" << QString::fromStdString( e.get_description() );
  }
}

void FTSResultsRequest::run()
{
  if ( !dict.ensureInitDone().empty() ) {
    setErrorString( QString::fromUtf8( dict.ensureInitDone().c_str() ) );
    finish();
    return;
  }

  try
  {
    if( dict.haveFTSIndex() )
    {
      //no need to parse the search string,  use xapian directly.
      //if the search mode is wildcard, change xapian search query flag?
      // Open the database for searching.
      Xapian::Database db(dict.ftsIndexName());

      // Start an enquire session.
      Xapian::Enquire enquire( db );

      // Combine the rest of the command line arguments with spaces between
      // them, so that simple queries don't have to be quoted at the shell
      // level.
      string query_string( searchString.toStdString() );

      // Parse the query string to produce a Xapian::Query object.
      Xapian::QueryParser qp;
      qp.set_database( db );
      Xapian::QueryParser::feature_flag flag = Xapian::QueryParser::FLAG_DEFAULT;
      if( searchMode == FTS::Wildcards )
        flag = Xapian::QueryParser::FLAG_WILDCARD;
      Xapian::Query query = qp.parse_query( query_string, flag|Xapian::QueryParser::FLAG_CJK_NGRAM );
      qDebug() << "Parsed query is: " << query.get_description().c_str();

      // Find the top 100 results for the query.
      enquire.set_query( query );
      Xapian::MSet matches = enquire.get_mset( 0, 100 );

      emit matchCount(matches.get_matches_estimated());
      // Display the results.
      qDebug() << matches.get_matches_estimated() << " results found.\n";
      qDebug() << "Matches " << matches.size() << ":\n\n";
      QList< uint32_t > offsetsForHeadwords;
      for( Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i )
      {
        qDebug() << i.get_rank() + 1 << ": " << i.get_weight() << " docid=" << *i << " ["
                 << i.get_document().get_data().c_str() << "]";
        if(i.get_document().get_data()==finish_mark)
          continue;
        offsetsForHeadwords.append( atoi( i.get_document().get_data().c_str() ) );
      }

      if( !offsetsForHeadwords.isEmpty() )
      {
        QVector< QString > headwords;
        QMutexLocker _( &dataMutex );
        QString id = QString::fromUtf8( dict.getId().c_str() );
        dict.getHeadwordsFromOffsets( offsetsForHeadwords, headwords, &isCancelled );
        for(const auto & headword : headwords)
        {
          foundHeadwords->append( FTS::FtsHeadword( headword, id, QStringList(), matchCase ) );
        }
      }
    }
    else {
      //if no fulltext index,just returned.
      qWarning() << "There is no fulltext index right now.";
      finish();
      return;
    }

    if ( foundHeadwords && !foundHeadwords->empty() ) {
      QMutexLocker _( &dataMutex );
      data.resize( sizeof( foundHeadwords ) );
      memcpy( &data.front(), &foundHeadwords, sizeof( foundHeadwords ) );
      foundHeadwords = nullptr;
      hasAnyData     = true;
    }
  }
  catch (const Xapian::Error &e) {
    qWarning() << e.get_description().c_str();
  }
  catch( std::exception &ex )
  {
    gdWarning( "FTS: Failed full-text search for \"%s\", reason: %s\n",
               dict.getName().c_str(), ex.what() );
    // Results not loaded -- we don't set the hasAnyData flag then
  }

  finish();
}

} // namespace

