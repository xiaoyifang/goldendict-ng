#pragma once

#include "article_netmgr.hh"

class IframeSchemeHandler: public QWebEngineUrlSchemeHandler
{
  Q_OBJECT

public:
  IframeSchemeHandler( QObject * parent = nullptr );
  void requestStarted( QWebEngineUrlRequestJob * requestJob );

protected:

private:
  QNetworkAccessManager mgr;
};
