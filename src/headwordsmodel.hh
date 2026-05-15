#pragma once

#include "dict/dictionary.hh"
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
  QString getCurrentWord() const;
  bool containWord( const QString & word );
  QSet< QString > getRemainRows( QString & lastWord );
  void setMaxFilterResults( int _maxFilterResults );
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
  QString lastWord;
  char * ptr;
  QMutex lock;
};
