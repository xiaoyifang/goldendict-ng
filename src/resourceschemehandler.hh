#ifndef RESOURCESCHEMEHANDLER_H
#define RESOURCESCHEMEHANDLER_H

#include "article_netmgr.hh"

class ResourceSchemeHandler: public QWebEngineUrlSchemeHandler
{
  Q_OBJECT

public:
  ResourceSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent = nullptr );
  void requestStarted( QWebEngineUrlRequestJob * requestJob );

protected:
  void replyJob( sptr< Dictionary::DataRequest > reply, QWebEngineUrlRequestJob * requestJob, QString content_type );

private:
  ArticleNetworkAccessManager & mManager;
  QMimeDatabase db;
};

#endif // RESOURCESCHEMEHANDLER_H
