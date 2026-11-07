#pragma once

#include <QWebEnginePage>

#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
  #include <QWebEnginePermission>
#endif

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
  // Consumes one pending loadFinished(false) caused by rejecting an internal
  // audio/video link navigation (played natively, not navigated). Returns
  // false for genuine load failures.
  bool takeResourceClickRejection();
signals:
  void linkClicked( const QUrl & url );

private slots:
#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
  void onPermissionRequested( const QWebEnginePermission & permission );
#endif

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
  int pendingResourceRejections = 0;
  bool isPopup = false;
};
