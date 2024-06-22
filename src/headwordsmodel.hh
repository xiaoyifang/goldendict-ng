#ifndef HEADWORDSMODEL_H
#define HEADWORDSMODEL_H

#include "dict/dictionary.hh"

#include <QAbstractListModel>
#include <QStringList>

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
  void setFilter( QRegularExpression );
  void appendWord( const QString & word );
  int getCurrentIndex() const;
  bool containWord( const QString & word );
  QSet< QString > getRemainRows( int & nodeIndex );
  void setMaxFilterResults( int _maxFilterResults );
signals:
  void numberPopulated( int number );

public slots:
  void setDict( Dictionary::Class * dict );
  void requestFinished();

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
  std::list< sptr< Dictionary::WordSearchRequest > > queuedRequests;
};

#endif // HEADWORDSMODEL_H
