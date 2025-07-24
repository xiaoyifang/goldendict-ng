#pragma once

#include "article_netmgr.hh"

class ResourceSchemeHandler: public QWebEngineUrlSchemeHandler
{
  Q_OBJECT

public:
  ResourceSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent = nullptr );
  void requestStarted( QWebEngineUrlRequestJob * requestJob );

protected:
  void replyJob( sptr< ResourceRequest > reply, QWebEngineUrlRequestJob * requestJob, QString content_type );

private:
  ArticleNetworkAccessManager & mManager;
  QMimeDatabase db;
};
