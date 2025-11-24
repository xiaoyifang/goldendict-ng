#pragma once

#include <QWebEnginePage>

struct LastReqInfo
{
  QString group;
  QString mutedDicts;
};

class ArticleWebPage: public QWebEnginePage
{
  Q_OBJECT

public:
  explicit ArticleWebPage( QObject * parent = nullptr );
signals:
  void linkClicked( const QUrl & url );

protected:
  virtual bool acceptNavigationRequest( const QUrl & url, NavigationType type, bool isMainFrame ) override;
  virtual void javaScriptAlert( const QUrl & securityOrigin, const QString & msg ) override;
  virtual bool javaScriptConfirm( const QUrl & securityOrigin, const QString & msg ) override;
  virtual bool javaScriptPrompt( const QUrl & securityOrigin,
                                 const QString & msg,
                                 const QString & defaultValue,
                                 QString * result ) override;

private:
  LastReqInfo lastReq;
};
