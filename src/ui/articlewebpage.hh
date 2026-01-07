#pragma once

#include <QWebEnginePage>

struct LastReqInfo
{
  QString group;
  bool isPopup = false;
};

class ArticleWebPage: public QWebEnginePage
{
  Q_OBJECT

public:
  explicit ArticleWebPage( QObject * parent = nullptr, bool isPopup_ = false );
  void setPopup( bool popup )
  {
    isPopup = popup;
  }
signals:
  void linkClicked( const QUrl & url );

protected:
  bool acceptNavigationRequest( const QUrl & url, NavigationType type, bool isMainFrame ) override;
  void javaScriptAlert( const QUrl & securityOrigin, const QString & msg ) override;
  bool javaScriptConfirm( const QUrl & securityOrigin, const QString & msg ) override;
  bool javaScriptPrompt( const QUrl & securityOrigin,
                         const QString & msg,
                         const QString & defaultValue,
                         QString * result ) override;
  void javaScriptConsoleMessage( JavaScriptConsoleMessageLevel level,
                                 const QString & message,
                                 int lineNumber,
                                 const QString & sourceID ) override;

private:
  LastReqInfo lastReq;
};
