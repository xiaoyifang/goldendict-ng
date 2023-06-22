#ifndef __FTSHELPERS_HH_INCLUDED__
#define __FTSHELPERS_HH_INCLUDED__

#include <QString>
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QtCore5Compat/QRegExp>
#else
#include <QRegExp>
#endif
#include <QList>
#include <QtConcurrent>

#include "dict/dictionary.hh"
#include "btreeidx.hh"
#include "fulltextsearch.hh"
#include "folding.hh"
#include "wstring_qt.hh"

namespace FtsHelpers
{

bool ftsIndexIsOldOrBad( BtreeIndexing::BtreeDictionary * dict );

void makeFTSIndex( BtreeIndexing::BtreeDictionary * dict, QAtomicInt & isCancelled );

class FTSResultsRequest : public Request::Blob
{
  BtreeIndexing::BtreeDictionary & dict;

  QString searchString;
  int searchMode;
  bool matchCase;

  QAtomicInt isCancelled;

  QAtomicInt results;
  QFuture< void > f;

  QList< FTS::FtsHeadword > * foundHeadwords;

public:

  FTSResultsRequest( BtreeIndexing::BtreeDictionary & dict_,
                     QString const & searchString_,
                     int searchMode_,
                     bool matchCase_,
                     bool ignoreDiacritics_ ):
    dict( dict_ ),
    searchString( searchString_ ),
    searchMode( searchMode_ ),
    matchCase( matchCase_ )
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
