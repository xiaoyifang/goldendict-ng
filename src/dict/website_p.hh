#pragma once
#include "dictionary.hh"
#include "config.hh"
#include <QTextCodec>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace WebSite {

class WebSiteArticleRequest: public Request::Article
{
  Q_OBJECT
  QNetworkReply * netReply;
  QString url;
  Dictionary::Class * dictPtr;
  QNetworkAccessManager & mgr;

public:

  WebSiteArticleRequest( QString const & url, QNetworkAccessManager & _mgr, Dictionary::Class * dictPtr_ );
  ~WebSiteArticleRequest() {}

  void cancel() override;

private:

  static QTextCodec * codecForHtml( QByteArray const & ba );

public slots:
  void requestFinished( QNetworkReply * );
};


class WebSiteResourceRequest: public Request::Blob
{
  Q_OBJECT
  QNetworkReply * netReply;
  QString url;
  Dictionary::Class * dictPtr;
  QNetworkAccessManager & mgr;

public:

  WebSiteResourceRequest( QString const & url, QNetworkAccessManager & _mgr, Dictionary::Class * dictPtr_ );
  ~WebSiteResourceRequest() {}

  void cancel() override;

private:

  static QTextCodec * codecForHtml( QByteArray const & ba );

public slots:
  void requestFinished( QNetworkReply * );
};
} // namespace WebSite
