#include "iframeschemehandler.hh"

#include "iconv.hh"

IframeSchemeHandler::IframeSchemeHandler( QObject * parent ):
  QWebEngineUrlSchemeHandler( parent )
{
}
void IframeSchemeHandler::requestStarted( QWebEngineUrlRequestJob * requestJob )
{
  QUrl url = requestJob->requestUrl();

  // website dictionary iframe url
  url = QUrl( Utils::Url::queryItemValue( url, "url" ) );
  QNetworkRequest request;
  request.setUrl( url );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute,
                        QNetworkRequest::RedirectPolicy::NoLessSafeRedirectPolicy );

  QNetworkReply * reply = mgr.get( request );

  auto finishAction = [ = ]() {
    QByteArray contentType = "text/html";
    QString codecName;
    const auto ctHeader = reply->header( QNetworkRequest::ContentTypeHeader );
    if ( ctHeader.isValid() ) {
      contentType      = ctHeader.toByteArray();
      const auto ct    = ctHeader.toString();
      const auto index = ct.indexOf( "charset=" );
      if ( index > -1 ) {
        codecName = ct.mid( index + 8 );
      }
    }
    auto buffer = new QBuffer( requestJob );

    QByteArray replyData = reply->readAll();
    QString articleString;

    auto encoding = Iconv::findValidEncoding( { codecName } );
    if ( !encoding.isEmpty() ) {
      articleString = Iconv::toQString( encoding.toUtf8().constData(), replyData.data(), replyData.size() );
    }
    else {
      articleString = QString::fromUtf8( replyData );
    }
    // Handle reply data
    // 404 response may have response body.
    if ( reply->error() != QNetworkReply::NoError && articleString.isEmpty() ) {
      if ( reply->error() == QNetworkReply::ContentNotFoundError ) {
        buffer->deleteLater();
        //work around to fix QTBUG-106573
        requestJob->redirect( url );
        return;
      }
      QString emptyHtml = QString( "<html><body>%1</body></html>" ).arg( reply->errorString() );
      buffer->setData( emptyHtml.toUtf8() );
      requestJob->reply( contentType, buffer );
      return;
    }

    // Change links from relative to absolute

    QString root = reply->url().scheme() + "://" + reply->url().host();

    if ( reply->url().port() != 80 && reply->url().port() != 443 && reply->url().port() != -1 ) {
      root = root + ":" + QString::number( reply->url().port() );
    }
    QString base = root + reply->url().path();


    QRegularExpression baseTag( R"EOF(<base\s+href=["'](.*?)["'].*?>)EOF",
                                QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::DotMatchesEverythingOption );


    if ( const auto match = baseTag.match( articleString ); match.hasMatch() ) {
      base = reply->url().resolved( match.captured( 1 ) ).url();
    }

    QString baseTagHtml = QString( R"(<base href="%1">)" ).arg( base );

    QString depressionFocus =
      R"(<script type="application/javascript"> HTMLElement.prototype.focus=function(){console.log("focus() has been disabled.");}</script>
<script type="text/javascript" src="qrc:///scripts/iframeResizer.contentWindow.min.js">
</script><script type="text/javascript" src="qrc:///scripts/iframe-defer.js"></script>)";

    articleString.remove( baseTag );

    articleString.replace( "window.location", "window.location;_window_location" );

    QRegularExpression headTag( R"(<head\b.*?>)",
                                QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::DotMatchesEverythingOption );
    auto match = headTag.match( articleString );
    if ( match.hasMatch() ) {
      articleString.insert( match.capturedEnd(), baseTagHtml );
      articleString.insert( match.capturedEnd(), depressionFocus );
    }
    else {
      // the html contain no head element
      // just insert at the beginning of the html ,and leave it at the mercy of browser(chrome webengine)
      articleString.insert( 0, baseTagHtml );
      articleString.insert( 0, depressionFocus );
    }

    buffer->setData( articleString.toUtf8() );

    requestJob->reply( "text/html; charset=utf-8", buffer );

  };
  connect( reply, &QNetworkReply::finished, requestJob, finishAction );

  connect( requestJob, &QObject::destroyed, reply, &QObject::deleteLater );
}
