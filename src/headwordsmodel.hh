#pragma once

#include "dict/dictionary.hh"
#include "headwordscache.hh"
#include <QAbstractListModel>

static const int HEADWORDS_MAX_LIMIT = 500000;
class HeadwordListModel: public QAbstractListModel
{
  Q_OBJECT

public:
  HeadwordListModel( QObject * parent = nullptr );

  int rowCount( const QModelIndex & parent = QModelIndex() ) const override;
  int totalCount() const;
  int wordCount() const;
  bool isFinish() const;
  QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const override;
  QString getRow( int row );
  void setFilter( const QRegularExpression & );
  void appendWord( const QString & word );
  int getCurrentIndex() const;
  bool containWord( const QString & word );
  QSet< QString > getRemainRows( int & nodeIndex );
  void setMaxFilterResults( int _maxFilterResults );

  /// Returns true if we're reading from the mmap'd headword cache file
  /// (fast path). False means we're using the legacy btree traversal
  /// (slow path) because no cache is available yet.
  bool usingCache() const
  {
    return cacheMode;
  }

  /// Re-check the cache file (called after a background build completes).
  /// If the cache is now available, switches to cache mode and resets the model.
  void refreshCache();

signals:
  void numberPopulated( int number );

public slots:
  void setDict( Dictionary::Class * dict );
  void requestFinished( const sptr< Dictionary::WordSearchRequest > & );

protected:
  bool canFetchMore( const QModelIndex & parent ) const override;
  void fetchMore( const QModelIndex & parent ) override;

private:
  QStringList words;
  QStringList original_words;
  QSet< QString > hashedWords;
  QStringList filterWords;
  bool filtering;
  QStringList fileSortedList;
  long totalSize;
  int maxFilterResults;
  bool finished;
  Dictionary::Class * _dict;
  int index;
  char * ptr;
  QMutex lock;

  // Headword cache (mmap'd). When valid, all reads go through this.
  HeadwordCache::HeadwordCache hwCache;
  bool cacheMode = false;

  // Active filter range in cache mode (half-open [filterBegin, filterEnd)).
  // When no filter is active both are 0, meaning "show all".
  uint32_t filterBegin = 0;
  uint32_t filterEnd   = 0;
};
