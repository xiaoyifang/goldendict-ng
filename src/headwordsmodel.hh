#pragma once

#include "dict/dictionary.hh"
#include "headwordindex.hh"
#include <QAbstractListModel>

static const int HEADWORDS_MAX_LIMIT = 500000;
static const int HEADWORD_PAGE_SIZE  = 1000;

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

  /// Check if the dictionary has a headword index available
  bool hasHeadwordIndex() const;

  /// Check if the dictionary is large enough to benefit from headword index
  bool isLargeDictionary() const;

  /// Check if index building is recommended (large dict without index)
  bool isIndexBuildRecommended() const;

  /// Build headword index for the current dictionary (if not already built)
  /// @param autoBuild If true, will be called automatically for large dictionaries
  void buildHeadwordIndex( bool autoBuild = false );

  /// Cancel ongoing index build
  void cancelIndexBuild();

  /// Check if index is currently being built
  bool isIndexBuilding() const;

  /// Search headwords using prefix matching (uses index if available)
  void searchPrefix( const QString & prefix, int offset, int limit );

  /// Search headwords using wildcard pattern (uses index if available)
  void searchWildcard( const QString & pattern, int offset, int limit );

signals:
  void numberPopulated( int number );
  void indexBuildProgress( int percent );
  void indexBuildFinished( bool success );
  /// Emitted when the dictionary is large and would benefit from an index
  void indexBuildRecommended();

public slots:
  void setDict( Dictionary::Class * dict );
  void requestFinished( const sptr< Dictionary::WordSearchRequest > & );

protected:
  bool canFetchMore( const QModelIndex & parent ) const override;
  void fetchMore( const QModelIndex & parent ) override;

private:
  /// Try to use headword index for fetching, returns true if successful
  bool fetchFromIndex( int offset, int limit );

  /// Fallback to legacy btree-based fetching
  void fetchFromBtree();

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

  // Index-based pagination state
  int currentOffset;
  bool useIndex;
  QString currentSearchPrefix;
  QAtomicInt indexBuildCancelled;
  QAtomicInt indexBuilding;
};
