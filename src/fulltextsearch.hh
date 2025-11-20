#pragma once

#include <QTimer>
#include <QRunnable>
#include <QSemaphore>
#include "dict/dictionary.hh"
#include "ui_fulltextsearch.h"
#include "config.hh"
#include "instances.hh"
#include "delegate.hh"

namespace FTS {

enum {
  // Minimum word length for indexing
  MinimumWordSize = 4,

  // Maximum dictionary size for first iteration of FTS indexing
  MaxDictionarySizeForFastSearch = 150000,

  // Maxumum match length for highlight search results
  // (QWebEnginePage::findText() crashes on too long strings)
  MaxMatchLengthForHighlightResults = 500
};

enum SearchMode {
  WholeWords = 0, // aka Default search using Xapian query syntax
  PlainText,
  Wildcards,
  RegExp
};

struct FtsHeadword
{
  QString headword;
  QStringList dictIDs;
  QStringList foundHiliteRegExps;
  bool matchCase;

  FtsHeadword( const QString & headword_, const QString & dictid_, QStringList hilites, bool match_case ):
    headword( headword_ ),
    foundHiliteRegExps( hilites ),
    matchCase( match_case )
  {
    dictIDs.append( dictid_ );
  }

  bool operator<( const FtsHeadword & other ) const;

  bool operator==( const FtsHeadword & other ) const
  {
    return headword.compare( other.headword, Qt::CaseInsensitive ) == 0;
  }

  bool operator!=( const FtsHeadword & other ) const
  {
    return headword.compare( other.headword, Qt::CaseInsensitive ) != 0;
  }
};

class Indexing: public QObject, public QRunnable
{
  Q_OBJECT

  QAtomicInt & isCancelled;
  const std::vector< sptr< Dictionary::Class > > & dictionaries;
  QSemaphore & hasExited;
  QTimer timer;

public:
  Indexing( QAtomicInt & cancelled, const std::vector< sptr< Dictionary::Class > > & dicts, QSemaphore & hasExited_ ):
    isCancelled( cancelled ),
    dictionaries( dicts ),
    hasExited( hasExited_ )
  {
    // The timer will run in the thread that executes Indexing::run()
    setAutoDelete( true ); // Ensure QThreadPool deletes this instance
    timer.setInterval( 2000 );
    connect( &timer, &QTimer::timeout, this, &Indexing::timeout );
  }

  ~Indexing()
  {
    emit sendNowIndexingName( QString() );
    hasExited.release();
  }

  virtual void run();

signals:
  void sendNowIndexingName( QString );

private slots:
  void timeout();
};

class FtsIndexing: public QObject
{
  Q_OBJECT

public:
  FtsIndexing( const std::vector< sptr< Dictionary::Class > > & dicts );
  virtual ~FtsIndexing()
  {
    stopIndexing();
  }

  void setDictionaries( const std::vector< sptr< Dictionary::Class > > & dicts )
  {
    clearDictionaries();
    dictionaries = dicts;
  }

  void clearDictionaries()
  {
    dictionaries.clear();
  }

  /// Start dictionaries indexing for full-text search
  void doIndexing();

  /// Break indexing thread
  void stopIndexing();

  QString nowIndexingName();

private:
  QAtomicInt isCancelled;
  QSemaphore indexingExited;
  std::vector< sptr< Dictionary::Class > > dictionaries;
  bool started;
  QString nowIndexing;
  QMutex nameMutex;

private slots:
  void setNowIndexedName( const QString & name );

signals:
  void newIndexingName( QString name );
};

/// A model to be projected into the view, according to Qt's MVC model
class HeadwordsListModel: public QAbstractListModel
{
  Q_OBJECT

public:

  HeadwordsListModel( QWidget * parent,
                      QList< FtsHeadword > & headwords_,
                      const std::vector< sptr< Dictionary::Class > > & dicts ):
    QAbstractListModel( parent ),
    headwords( headwords_ ),
    dictionaries( dicts )
  {
  }

  int rowCount( const QModelIndex & parent ) const;
  QVariant data( const QModelIndex & index, int role ) const;

  //  bool insertRows( int row, int count, const QModelIndex & parent );
  //  bool removeRows( int row, int count, const QModelIndex & parent );
  //  bool setData( QModelIndex const & index, const QVariant & value, int role );

  void addResults( const QModelIndex & parent, const QList< FtsHeadword > & headwords );
  bool clear();

private:

  QList< FtsHeadword > & headwords;
  const std::vector< sptr< Dictionary::Class > > & dictionaries;

  int getDictIndex( const QString & id ) const;

signals:
  void contentChanged();
};

class FullTextSearchDialog: public QDialog
{
  Q_OBJECT

  Config::Class & cfg;
  const std::vector< sptr< Dictionary::Class > > & dictionaries;
  const std::vector< Instances::Group > & groups;
  unsigned group;
  std::vector< sptr< Dictionary::Class > > activeDicts;

  std::list< sptr< Dictionary::DataRequest > > searchReqs;

  FtsIndexing & ftsIdx;

  QRegularExpression searchRegExp;
  int matchedCount;

public:
  FullTextSearchDialog( QWidget * parent,
                        Config::Class & cfg_,
                        const std::vector< sptr< Dictionary::Class > > & dictionaries_,
                        const std::vector< Instances::Group > & groups_,
                        FtsIndexing & ftsidx );
  virtual ~FullTextSearchDialog();

  void setSearchText( const QString & text );

  void setCurrentGroup( unsigned group_ )
  {
    group = group_;
    updateDictionaries();
  }

  void stopSearch();

protected:
  bool eventFilter( QObject * obj, QEvent * ev );

private:
  Ui::FullTextSearchDialog ui;
  QList< FtsHeadword > results;
  HeadwordsListModel * model;
  WordListItemDelegate * delegate;
  QAction helpAction;

  void showDictNumbers();

private slots:
  void setNewIndexingName( QString );
  void saveData();
  void accept();
  void searchReqFinished();
  void matchCount( int );
  void reject();
  void itemClicked( const QModelIndex & idx );
  void updateDictionaries();

signals:
  void showTranslationFor( const QString &,
                           const QStringList & dictIDs,
                           const QRegularExpression & searchRegExp,
                           bool ignoreDiacritics );
  void closeDialog();
};


} // namespace FTS
