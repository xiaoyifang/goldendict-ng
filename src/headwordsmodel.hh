#ifndef HEADWORDSMODEL_H
#define HEADWORDSMODEL_H

#include "dict/dictionary.hh"

#include <QAbstractListModel>
#include <QStringList>

class HeadwordListModel : public QAbstractListModel
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
  void setFilter( QRegularExpression );
  void addMatches( QStringList matches );
  int getCurrentIndex();
  QSet< QString > getRemainRows( int & nodeIndex );
signals:
  void numberPopulated( int number );
  void finished( int number );

public slots:
  void setDict( Dictionary::Class * dict );
  void requestFinished();

protected:
  bool canFetchMore( const QModelIndex & parent ) const override;
  void fetchMore( const QModelIndex & parent ) override;

private:
  QStringList words;
  QStringList filterWords;
  bool filtering;
  QStringList fileSortedList;
  long totalSize;
  Dictionary::Class * _dict;
  int index;
  char * ptr;
  Mutex lock;
  std::list< sptr< Dictionary::WordSearchRequest > > queuedRequests;
};

#endif // HEADWORDSMODEL_H
