#pragma once

#include "article_netmgr.hh"

class ResourceSchemeHandler: public QWebEngineUrlSchemeHandler
{
  Q_OBJECT

public:
  ResourceSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent = nullptr );
  void requestStarted( QWebEngineUrlRequestJob * requestJob );

protected:
  void replyJob( sptr< Dictionary::DataRequest > reply, QWebEngineUrlRequestJob * requestJob );

private:
  ArticleNetworkAccessManager & mManager;
};
