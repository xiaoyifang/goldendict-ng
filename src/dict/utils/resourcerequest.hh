#pragma once
#include <QtConcurrentRun>

/// Asynchronous Dictionary Request helper.
/// Pretty much a wrapper of QFuture
/// This can be used syn
class ResourceRequest: public QObject
{
  Q_OBJECT

public:
  QFuture< void > f;
  QMutex dataMutex;
  std::vector< char > data;
  bool hasAnyData = false;
  QString errorString;

  /// A bloated data holder invented because of OOP.
  /// It simply has a cancelled QFuture that indicates that it is finished.
  /// The user is supposed to fill in data synchronously.
  /// @return
  static std::shared_ptr< ResourceRequest > NoDataFinished( bool succeed );

  virtual ~ResourceRequest();

  virtual void cancel();
  virtual void finish();
  virtual bool isFinished();
  void update();

  /// TODO: is this useful?
  virtual void setErrorString( const QString & str );


  qsizetype dataSize();
  void appendString( std::string_view str );
  void appendDataSlice( const void * buffer, size_t size );
  void getDataSlice( size_t offset, size_t size, void * buffer );
  std::vector< char > & getFullData();
signals:
  void finished();

  /// This signal is emitted when more data becomes available. Local
  /// dictionaries don't call this, since it is preferred that all
  /// data would be available from them at once, but network dictionaries
  /// might call this.
  void updated();
};
