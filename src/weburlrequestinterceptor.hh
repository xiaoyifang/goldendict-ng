#pragma once

#include <QWebEngineUrlRequestInterceptor>

class WebUrlRequestInterceptor: public QWebEngineUrlRequestInterceptor
{
  Q_OBJECT

public:
  WebUrlRequestInterceptor( QObject * p );
  void interceptRequest( QWebEngineUrlRequestInfo & info );
signals:
  void linkClicked( const QUrl & url );
};
