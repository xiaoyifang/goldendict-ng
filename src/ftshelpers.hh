#ifndef __FTSHELPERS_HH_INCLUDED__
#define __FTSHELPERS_HH_INCLUDED__

#include <QString>
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QtCore5Compat/QRegExp>
#else
#include <QRegExp>
#endif
#include <QRunnable>
#include <QSemaphore>
#include <QList>
#include <QtConcurrent>

#include "dict/dictionary.hh"
#include "btreeidx.hh"
#include "fulltextsearch.hh"
#include "chunkedstorage.hh"
#include "folding.hh"
#include "wstring_qt.hh"

#include <string>

namespace FtsHelpers
{

bool ftsIndexIsOldOrBad( std::string const & indexFile,
                         BtreeIndexing::BtreeDictionary * dict );

bool parseSearchString( QString const & str, QStringList & IndexWords,
                        QStringList & searchWords,
                        QRegExp & searchRegExp, int searchMode,
                        bool matchCase,
                        int distanceBetweenWords,
                        bool & hasCJK,
                        bool ignoreWordsOrder = false );

void makeFTSIndex( BtreeIndexing::BtreeDictionary * dict, QAtomicInt & isCancelled );
bool isCJKChar( ushort ch );

class FTSResultsRequest : public Dictionary::DataRequest
{
  BtreeIndexing::BtreeDictionary & dict;

  QString searchString;
  int searchMode;
  bool matchCase;
  int distanceBetweenWords;
  int maxResults;
  bool ignoreWordsOrder;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;

  QAtomicInt results;
  QFuture< void > f;

  QList< FTS::FtsHeadword > * foundHeadwords;

public:

  FTSResultsRequest( BtreeIndexing::BtreeDictionary & dict_,
                     QString const & searchString_,
                     int searchMode_,
                     bool matchCase_,
                     int distanceBetweenWords_,
                     int maxResults_,
                     bool ignoreWordsOrder_,
                     bool ignoreDiacritics_ ):
    dict( dict_ ),
    searchString( searchString_ ),
    searchMode( searchMode_ ),
    matchCase( matchCase_ ),
    distanceBetweenWords( distanceBetweenWords_ ),
    maxResults( maxResults_ ),
    ignoreWordsOrder( ignoreWordsOrder_ ),
    ignoreDiacritics( ignoreDiacritics_ )
  {
    if( ignoreDiacritics_ )
      searchString = QString::fromStdU32String( Folding::applyDiacriticsOnly( gd::removeTrailingZero( searchString_ ) ) );

    foundHeadwords = new QList< FTS::FtsHeadword >;
    results         = 0;
    f              = QtConcurrent::run( [ this ]() { this->run(); } );
  }

  void run();
  virtual void cancel()
  {
    isCancelled.ref();
  }

  ~FTSResultsRequest()
  {
    isCancelled.ref();
    f.waitForFinished();

    delete foundHeadwords;
  }
};

} // namespace

#endif // __FTSHELPERS_HH_INCLUDED__
