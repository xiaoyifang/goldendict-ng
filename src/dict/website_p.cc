#include "website_p.hh"
#include "gddebug.hh"

namespace WebSite {
void WebSiteArticleRequest::cancel()
{
  finish();
}

WebSiteArticleRequest::WebSiteArticleRequest( QString const & url_,
                                              QNetworkAccessManager & _mgr,
                                              Dictionary::Class * dictPtr_ ):
  url( url_ ),
  dictPtr( dictPtr_ ),
  mgr( _mgr )
{
  connect( &mgr,
           SIGNAL( finished( QNetworkReply * ) ),
           this,
           SLOT( requestFinished( QNetworkReply * ) ),
           Qt::QueuedConnection );

  QUrl reqUrl( url );

  auto request = QNetworkRequest( reqUrl );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  netReply = mgr.get( request );

#ifndef QT_NO_SSL
  connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
}

QTextCodec * WebSiteArticleRequest::codecForHtml( QByteArray const & ba )
{
  return QTextCodec::codecForHtml( ba, 0 );
}

void WebSiteArticleRequest::requestFinished( QNetworkReply * r )
{
  if ( isFinished() ) // Was cancelled
    return;

  if ( r != netReply ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  if ( netReply->error() == QNetworkReply::NoError ) {
    // Check for redirect reply

    QVariant possibleRedirectUrl = netReply->attribute( QNetworkRequest::RedirectionTargetAttribute );
    QUrl redirectUrl             = possibleRedirectUrl.toUrl();
    if ( !redirectUrl.isEmpty() ) {
      disconnect( netReply, 0, 0, 0 );
      netReply->deleteLater();
      netReply = mgr.get( QNetworkRequest( redirectUrl ) );
#ifndef QT_NO_SSL
      connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
      return;
    }

    // Handle reply data

    QByteArray replyData = netReply->readAll();
    QString articleString;

    QTextCodec * codec = WebSiteArticleRequest::codecForHtml( replyData );
    if ( codec )
      articleString = codec->toUnicode( replyData );
    else
      articleString = QString::fromUtf8( replyData );

    // Change links from relative to absolute

    QString root = netReply->url().scheme() + "://" + netReply->url().host();
    QString base = root + netReply->url().path();
    while ( !base.isEmpty() && !base.endsWith( "/" ) )
      base.chop( 1 );

    QRegularExpression tags( R"(<\s*(a|link|img|script)\s+[^>]*(src|href)\s*=\s*['"][^>]+>)",
                             QRegularExpression::CaseInsensitiveOption );
    QRegularExpression links( R"(\b(src|href)\s*=\s*(['"])([^'"]+['"]))", QRegularExpression::CaseInsensitiveOption );
    int pos = 0;
    QString articleNewString;
    QRegularExpressionMatchIterator it = tags.globalMatch( articleString );
    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString tag = match.captured();

      QRegularExpressionMatch match_links = links.match( tag );
      if ( !match_links.hasMatch() ) {
        articleNewString += tag;
        continue;
      }

      QString url = match_links.captured( 3 );

      if ( url.indexOf( ":/" ) >= 0 || url.indexOf( "data:" ) >= 0 || url.indexOf( "mailto:" ) >= 0
           || url.startsWith( "#" ) || url.startsWith( "javascript:" ) ) {
        // External link, anchor or base64-encoded data
        articleNewString += tag;
        continue;
      }

      QString newUrl = match_links.captured( 1 ) + "=" + match_links.captured( 2 );
      if ( url.startsWith( "//" ) )
        newUrl += netReply->url().scheme() + ":";
      else if ( url.startsWith( "/" ) )
        newUrl += root;
      else
        newUrl += base;
      newUrl += match_links.captured( 3 );

      tag.replace( match_links.capturedStart(), match_links.capturedLength(), newUrl );
      articleNewString += tag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // Redirect CSS links to own handler

    QString prefix = QString( "bres://" ) + dictPtr->getId().c_str() + "/";
    QRegularExpression linkTags(
      R"((<\s*link\s[^>]*rel\s*=\s*['"]stylesheet['"]\s+[^>]*href\s*=\s*['"])([^'"]+)://([^'"]+['"][^>]+>))",
      QRegularExpression::CaseInsensitiveOption );
    pos = 0;
    it  = linkTags.globalMatch( articleString );

    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString newTag = match.captured( 1 ) + prefix + match.captured( 2 ) + "/" + match.captured( 3 );
      articleNewString += newTag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // See Issue #271: A mechanism to clean-up invalid HTML cards.
    articleString += QString::fromStdString( Utils::Html::getHtmlCleaner() );

    QByteArray articleBody = articleString.toUtf8();

    QString divStr = QString( "<div class=\"website\"" );
    divStr += dictPtr->isToLanguageRTL() ? " dir=\"rtl\">" : ">";

    articleBody.prepend( divStr.toUtf8() );
    articleBody.append( "</div>" );

    articleBody.prepend( "<div class=\"website_padding\"></div>" );

    QMutexLocker _( &dataMutex );

    size_t prevSize = data.size();

    data.resize( prevSize + articleBody.size() );

    memcpy( &data.front() + prevSize, articleBody.data(), articleBody.size() );

    hasAnyData = true;
  }
  else {
    if ( netReply->url().scheme() == "file" ) {
      gdWarning( "WebSites: Failed loading article from \"%s\", reason: %s\n",
                 dictPtr->getName().c_str(),
                 netReply->errorString().toUtf8().data() );
    }
    else {
      setErrorString( netReply->errorString() );
    }
  }

  disconnect( netReply, 0, 0, 0 );
  netReply->deleteLater();

  finish();
}

// =====================


void WebSiteResourceRequest::cancel()
{
  finish();
}

WebSiteResourceRequest::WebSiteResourceRequest( QString const & url_,
                                              QNetworkAccessManager & _mgr,
                                              Dictionary::Class * dictPtr_ ):
  url( url_ ),
  dictPtr( dictPtr_ ),
  mgr( _mgr )
{
  connect( &mgr,
           SIGNAL( finished( QNetworkReply * ) ),
           this,
           SLOT( requestFinished( QNetworkReply * ) ),
           Qt::QueuedConnection );

  QUrl reqUrl( url );

  auto request = QNetworkRequest( reqUrl );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  netReply = mgr.get( request );

#ifndef QT_NO_SSL
  connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
}

QTextCodec * WebSiteResourceRequest::codecForHtml( QByteArray const & ba )
{
  return QTextCodec::codecForHtml( ba, 0 );
}

void WebSiteResourceRequest::requestFinished( QNetworkReply * r )
{
  if ( isFinished() ) // Was cancelled
    return;

  if ( r != netReply ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  if ( netReply->error() == QNetworkReply::NoError ) {
    // Check for redirect reply

    QVariant possibleRedirectUrl = netReply->attribute( QNetworkRequest::RedirectionTargetAttribute );
    QUrl redirectUrl             = possibleRedirectUrl.toUrl();
    if ( !redirectUrl.isEmpty() ) {
      disconnect( netReply, 0, 0, 0 );
      netReply->deleteLater();
      netReply = mgr.get( QNetworkRequest( redirectUrl ) );
#ifndef QT_NO_SSL
      connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
      return;
    }

    // Handle reply data

    QByteArray replyData = netReply->readAll();
    QString articleString;

    QTextCodec * codec = WebSiteResourceRequest::codecForHtml( replyData );
    if ( codec )
      articleString = codec->toUnicode( replyData );
    else
      articleString = QString::fromUtf8( replyData );

    // Change links from relative to absolute

    QString root = netReply->url().scheme() + "://" + netReply->url().host();
    QString base = root + netReply->url().path();
    while ( !base.isEmpty() && !base.endsWith( "/" ) )
      base.chop( 1 );

    QRegularExpression tags( R"(<\s*(a|link|img|script)\s+[^>]*(src|href)\s*=\s*['"][^>]+>)",
                             QRegularExpression::CaseInsensitiveOption );
    QRegularExpression links( R"(\b(src|href)\s*=\s*(['"])([^'"]+['"]))", QRegularExpression::CaseInsensitiveOption );
    int pos = 0;
    QString articleNewString;
    QRegularExpressionMatchIterator it = tags.globalMatch( articleString );
    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString tag = match.captured();

      QRegularExpressionMatch match_links = links.match( tag );
      if ( !match_links.hasMatch() ) {
        articleNewString += tag;
        continue;
      }

      QString url = match_links.captured( 3 );

      if ( url.indexOf( ":/" ) >= 0 || url.indexOf( "data:" ) >= 0 || url.indexOf( "mailto:" ) >= 0
           || url.startsWith( "#" ) || url.startsWith( "javascript:" ) ) {
        // External link, anchor or base64-encoded data
        articleNewString += tag;
        continue;
      }

      QString newUrl = match_links.captured( 1 ) + "=" + match_links.captured( 2 );
      if ( url.startsWith( "//" ) )
        newUrl += netReply->url().scheme() + ":";
      else if ( url.startsWith( "/" ) )
        newUrl += root;
      else
        newUrl += base;
      newUrl += match_links.captured( 3 );

      tag.replace( match_links.capturedStart(), match_links.capturedLength(), newUrl );
      articleNewString += tag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // Redirect CSS links to own handler

    QString prefix = QString( "bres://" ) + dictPtr->getId().c_str() + "/";
    QRegularExpression linkTags(
      R"((<\s*link\s[^>]*rel\s*=\s*['"]stylesheet['"]\s+[^>]*href\s*=\s*['"])([^'"]+)://([^'"]+['"][^>]+>))",
      QRegularExpression::CaseInsensitiveOption );
    pos = 0;
    it  = linkTags.globalMatch( articleString );

    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString newTag = match.captured( 1 ) + prefix + match.captured( 2 ) + "/" + match.captured( 3 );
      articleNewString += newTag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // See Issue #271: A mechanism to clean-up invalid HTML cards.
    articleString += QString::fromStdString( Utils::Html::getHtmlCleaner() );

    QByteArray articleBody = articleString.toUtf8();

    QString divStr = QString( "<div class=\"website\"" );
    divStr += dictPtr->isToLanguageRTL() ? " dir=\"rtl\">" : ">";

    articleBody.prepend( divStr.toUtf8() );
    articleBody.append( "</div>" );

    articleBody.prepend( "<div class=\"website_padding\"></div>" );

    QMutexLocker _( &dataMutex );

    size_t prevSize = data.size();

    data.resize( prevSize + articleBody.size() );

    memcpy( &data.front() + prevSize, articleBody.data(), articleBody.size() );

    hasAnyData = true;
  }
  else {
    if ( netReply->url().scheme() == "file" ) {
      gdWarning( "WebSites: Failed loading article from \"%s\", reason: %s\n",
                 dictPtr->getName().c_str(),
                 netReply->errorString().toUtf8().data() );
    }
    else {
      setErrorString( netReply->errorString() );
    }
  }

  disconnect( netReply, 0, 0, 0 );
  netReply->deleteLater();

  finish();
}

} // namespace WebSite
