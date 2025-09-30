/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QtNetwork>
#include <QWebEngineUrlSchemeHandler>
#include <QNetworkAccessManager>

#include <utility>

#include "dict/dictionary.hh"
#include "article_maker.hh"

using std::vector;

/// A custom QNetworkAccessManager version which fetches images from the
/// dictionaries when requested.

// Proxy class for QNetworkReply to remove X-Frame-Options header
// It allow to show websites in <iframe> tag like Qt 4.x

class AllowFrameReply: public QNetworkReply
{
  Q_OBJECT

private:
  QNetworkReply * baseReply;
  QByteArray buffer;

  AllowFrameReply();
  AllowFrameReply( const AllowFrameReply & );

public:
  explicit AllowFrameReply( QNetworkReply * _reply );
  ~AllowFrameReply()
  {
    delete baseReply;
  }

  void close() override
  {
    baseReply->close();
  }

  // QIODevice virtual functions
  qint64 bytesAvailable() const override;
  bool atEnd() const override
  {
    return baseReply->atEnd();
  }
  qint64 bytesToWrite() const override
  {
    return baseReply->bytesToWrite();
  }
  bool canReadLine() const override
  {
    return baseReply->canReadLine();
  }
  bool isSequential() const override
  {
    return baseReply->isSequential();
  }
  bool waitForReadyRead( int msecs ) override
  {
    return baseReply->waitForReadyRead( msecs );
  }
  bool waitForBytesWritten( int msecs ) override
  {
    return baseReply->waitForBytesWritten( msecs );
  }
  bool reset() override
  {
    return baseReply->reset();
  }

public slots:
  void applyError( QNetworkReply::NetworkError code );

  // Redirect QNetworkReply slots
  void abort() override
  {
    baseReply->abort();
  }
  void ignoreSslErrors() override
  {
    baseReply->ignoreSslErrors();
  }

protected:
  // QNetworkReply virtual functions
  void ignoreSslErrorsImplementation( const QList< QSslError > & errors ) override
  {
    baseReply->ignoreSslErrors( errors );
  }
  void setSslConfigurationImplementation( const QSslConfiguration & configuration ) override
  {
    baseReply->setSslConfiguration( configuration );
  }
  void sslConfigurationImplementation( QSslConfiguration & configuration ) const override
  {
    configuration = baseReply->sslConfiguration();
  }

  // QIODevice virtual functions
  qint64 readData( char * data, qint64 maxSize ) override;
  qint64 readLineData( char * data, qint64 maxSize ) override
  {
    return baseReply->readLine( data, maxSize );
  }
  qint64 writeData( const char * data, qint64 maxSize ) override
  {
    return baseReply->write( data, maxSize );
  }
};

enum class ResourceType {
  UNTITLE,
  WELCOME,
  BLANK
};

class ArticleNetworkAccessManager: public QNetworkAccessManager
{
  Q_OBJECT
  const vector< sptr< Dictionary::Class > > & dictionaries;
  const ArticleMaker & articleMaker;
  const bool & disallowContentFromOtherSites;
  const bool & hideGoldenDictHeader;
  QMimeDatabase db;

public:

  ArticleNetworkAccessManager( QObject * parent,
                               const vector< sptr< Dictionary::Class > > & dictionaries_,
                               const ArticleMaker & articleMaker_,
                               const bool & disallowContentFromOtherSites_,
                               const bool & hideGoldenDictHeader_ ):
    QNetworkAccessManager( parent ),
    dictionaries( dictionaries_ ),
    articleMaker( articleMaker_ ),
    disallowContentFromOtherSites( disallowContentFromOtherSites_ ),
    hideGoldenDictHeader( hideGoldenDictHeader_ )
  {
  }

  /// Tries handling any kind of internal resources referenced by dictionaries.
  /// If it succeeds, the result is a dictionary request object. Otherwise, an
  /// empty pointer is returned.
  /// The function can optionally set the Content-Type header correspondingly.
  sptr< Dictionary::DataRequest > getResource( const QUrl & url, QString & contentType );

  virtual QNetworkReply * getArticleReply( const QNetworkRequest & req );
  string getHtml( ResourceType resourceType );
  
private:
  /// Check if the URL represents an image or other resource file, and redirect to appropriate protocol if needed.
  sptr< Dictionary::DataRequest > checkImageResource( const QUrl & url );
};

class ArticleResourceReply: public QNetworkReply
{
  Q_OBJECT

  sptr< Dictionary::DataRequest > req;
  qint64 alreadyRead;

  QAtomicInt finishSignalSent;

public:

  ArticleResourceReply( QObject * parent,
                        const QNetworkRequest &,
                        const sptr< Dictionary::DataRequest > &,
                        const QString & contentType );

  ~ArticleResourceReply();

protected:

  virtual qint64 bytesAvailable() const override;
  bool atEnd() const override;
  virtual void abort() override {}
  virtual qint64 readData( char * data, qint64 maxSize ) override;

  // We use the hackery below to work around the fact that we need to emit
  // ready/finish signals after we've been constructed.
signals:

  void readyReadSignal();
  void finishedSignal();

private slots:

  void reqUpdated();
  void reqFinished();

  void readyReadSlot();
  void finishedSlot();
};

class BlockedNetworkReply: public QNetworkReply
{
  Q_OBJECT

public:

  BlockedNetworkReply( QObject * parent );

  virtual qint64 readData( char *, qint64 )
  {
    return -1;
  }

  virtual void abort() {}

protected:

  // We use the hackery below to work around the fact that we need to emit
  // ready/finish signals after we've been constructed.

signals:

  void finishedSignal();

private slots:

  void finishedSlot();
};

class LocalSchemeHandler: public QWebEngineUrlSchemeHandler
{
  Q_OBJECT

public:
  LocalSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent = nullptr );
  void requestStarted( QWebEngineUrlRequestJob * requestJob );

protected:

private:
  ArticleNetworkAccessManager & mManager;
  QNetworkAccessManager mgr;
};
