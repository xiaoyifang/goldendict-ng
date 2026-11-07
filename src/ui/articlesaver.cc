/* Implementation of ArticleSaver helper to remove duplicated save logic.
 * This file centralizes the logic originally present in mainwindow.cc and
 * scanpopup.cc.
 */

#include "articlesaver.hh"

#include "ui/articleview.hh"
#include "config.hh"
#include <QStatusBar>

#include <QFileDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QWebEnginePage>
#include <QWebEngineDownloadRequest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>

using std::set;
using std::vector;
using std::pair;

namespace {
// Resource names are opaque dictionary keys fully controlled by the
// dictionary author, so a destination path built from them must never
// contain traversal sequences or Windows drive/ADS markers. Such paths get
// replaced with a generated, content-addressed name inside the save folder.
static bool hasUnsafeSegments( const QString & path )
{
  // Split on both separators: backslash traversal matters on Windows even
  // though URL paths formally use '/'.
  const QStringList segments = path.split( QRegularExpression( R"([\\/])" ), Qt::SkipEmptyParts );
  for ( const QString & segment : segments ) {
    if ( segment == QLatin1String( ".." ) || segment.contains( ':' ) ) {
      return true;
    }
  }
  return false;
}

// Helper moved from original implementations: collect resource urls and
// rewrite html to point to local files.
static void filterAndCollectResources( QString & html,
                                       QRegularExpression & rx,
                                       const QString & sep,
                                       const QString & folder,
                                       set< QByteArray > & resourceIncluded,
                                       vector< pair< QUrl, QString > > & downloadResources )
{
  int pos = 0;

  auto match = rx.match( html, pos );
  while ( match.hasMatch() ) {
    pos = match.capturedStart();
    QUrl url( match.captured( 1 ) );
    QString host         = url.host();
    QString resourcePath = Utils::Url::path( url );

    if ( !host.startsWith( '/' ) ) {
      host.insert( 0, '/' );
    }
    if ( !resourcePath.startsWith( '/' ) ) {
      resourcePath.insert( 0, '/' );
    }

    QCryptographicHash hash( QCryptographicHash::Md5 );
    hash.addData( match.captured().toUtf8() );

    // The destination "folder + host + resourcePath" must stay inside the
    // save folder. Canonicalize it and verify containment; anything that
    // would escape (traversal segments, drive letters, UNC paths) falls back
    // to a generated, content-addressed name so the saved article remains
    // self-contained without letting a dictionary write arbitrary files.
    const QString root      = QDir::cleanPath( folder );
    const QString candidate = QDir::cleanPath( folder + host + resourcePath );

    const bool contained = candidate.startsWith( root + '/' ) && !hasUnsafeSegments( host + resourcePath );

    QString destFileName, location;
    if ( contained ) {
      destFileName = candidate;
      resourcePath = QString::fromLatin1( QUrl::toPercentEncoding( resourcePath, "/" ) );
      location     = host + resourcePath;
    }
    else {
      destFileName = root + '/' + QString::fromLatin1( hash.result().toHex() );
      location     = '/' + QString::fromLatin1( hash.result().toHex() );
    }

    if ( resourceIncluded.insert( hash.result() ).second ) {
      // Gather resource information (url, filename) to be downloaded later
      downloadResources.emplace_back( url, destFileName );

      // Modify original url, set to the native one
      QString newUrl = sep + QDir( folder ).dirName() + location + sep;
      html.replace( pos, match.captured().length(), newUrl );
    }

    pos += match.captured().length();
    match = rx.match( html, pos );
  }
}
} // namespace

ArticleSaver::ArticleSaver( QWidget * uiParent, ArticleView * view, Config::Class & cfg ):

  uiParent_( uiParent ),
  view_( view ),
  cfg_( cfg )
{
}

ArticleSaver::~ArticleSaver() = default;

void ArticleSaver::save()
{
  if ( !view_ ) {
    qWarning() << "ArticleSaver::save called with null view";
    return;
  }

  QString fileName = view_->getTitle().simplified();

  // Replace reserved filename characters
  QRegularExpression rxName( R"([/\\\?\*:|<>])" );
  fileName.replace( rxName, "_" );

  fileName += ".html";
  QString savePath;

  if ( cfg_.articleSavePath.isEmpty() ) {
    savePath = QDir::homePath();
  }
  else {
    savePath = QDir::fromNativeSeparators( cfg_.articleSavePath );
    if ( !QDir( savePath ).exists() ) {
      savePath = QDir::homePath();
    }
  }

  QFileDialog::Options options = QFileDialog::HideNameFilterDetails;
  QString selectedFilter;
  QStringList filters;
  filters.push_back( QObject::tr( "Complete Html (*.html *.htm)" ) );
  filters.push_back( QObject::tr( "Single Html (*.html *.htm)" ) );
  filters.push_back( QObject::tr( "PDF document (*.pdf *.PDF)" ) );
  filters.push_back( QObject::tr( "Mime Html (*.mhtml)" ) );

  fileName = savePath + "/" + fileName;
  fileName = QFileDialog::getSaveFileName( uiParent_,
                                           QObject::tr( "Save Article As" ),
                                           fileName,
                                           filters.join( ";;" ),
                                           &selectedFilter,
                                           options );

  qDebug() << "filter:" << selectedFilter;
  const bool complete = filters.at( 0 ).startsWith( selectedFilter );

  if ( fileName.isEmpty() ) {
    return;
  }

  // PDF
  if ( filters.at( 2 ).startsWith( selectedFilter ) ) {
    QWebEnginePage * page = view_->page();
    QObject::connect( page, &QWebEnginePage::pdfPrintingFinished, this, [ this ]( const QString & fp, bool success ) {
      Q_UNUSED( fp )
      emit statusMessage( success ? QObject::tr( "Save PDF complete" ) : QObject::tr( "Save PDF failed" ), 5000 );
    } );

    page->printToPdf( fileName );
    return;
  }

  // MHTML
  if ( filters.at( 3 ).startsWith( selectedFilter ) ) {
    QWebEnginePage * page = view_->page();
    page->save( fileName, QWebEngineDownloadRequest::MimeHtmlSaveFormat );
    emit statusMessage( QObject::tr( "Save article complete" ), 5000 );
    return;
  }

  // Website handling
  if ( view_->isWebsite() ) {
    QWebEnginePage * page = view_->page();
    if ( filters.at( 0 ).startsWith( selectedFilter ) ) {
      page->save( fileName, QWebEngineDownloadRequest::CompleteHtmlSaveFormat );
    }
    else if ( filters.at( 1 ).startsWith( selectedFilter ) ) {
      page->save( fileName, QWebEngineDownloadRequest::SingleHtmlSaveFormat );
    }

    emit statusMessage( QObject::tr( "Save article complete" ), 5000 );
    return;
  }

  // Internal article -> get HTML and possibly collect resources
  view_->toHtml( [ this, fileName, complete, rxName ]( QString & html ) mutable {
    QFile file( fileName );
    if ( !file.open( QIODevice::WriteOnly ) ) {
      QMessageBox::critical( uiParent_,
                             QObject::tr( "Error" ),
                             QObject::tr( "Can't save article: %1" ).arg( file.errorString() ) );
    }
    else {
      QFileInfo fi( fileName );
      cfg_.articleSavePath = QDir::toNativeSeparators( fi.absoluteDir().absolutePath() );

      // Convert internal links
      static QRegularExpression rx3( R"lit(href="(bword:|gdlookup://localhost/)([^"]+)")lit" );
      int pos = 0;
      QRegularExpression anchorRx( "(g[0-9a-f]{32}_[0-9a-f]+_)" );
      auto match = rx3.match( html, pos );
      while ( match.hasMatch() ) {
        pos          = match.capturedStart();
        QString name = QUrl::fromPercentEncoding( match.captured( 2 ).simplified().toLatin1() );
        QString anchor;
        name.replace( "?gdanchor=", "#" );
        int n = name.indexOf( '#' );
        if ( n > 0 ) {
          anchor = name.mid( n );
          name.truncate( n );
          anchor.replace( anchorRx, "\\1" );
        }
        name.replace( rxName, "_" );
        name = QString( R"(href=")" ) + QUrl::toPercentEncoding( name ) + ".html" + anchor + "\"";
        html.replace( pos, match.captured().length(), name );
        pos += name.length();
        match = rx3.match( html, pos );
      }

      // MDict anchors
      QRegularExpression anchorLinkRe(
        R"((<\s*a\s+[^>]*\b(?:name|id)\b\s*=\s*["']*g[0-9a-f]{32}_)([0-9a-f]+_)(?=[^"']))",
        QRegularExpression::PatternOption::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption );
      html.replace( anchorLinkRe, "\\1" );

      if ( complete ) {
        QString folder = fi.absoluteDir().absolutePath() + "/" + fi.baseName() + "_files";
        static QRegularExpression rx1( R"lit("((?:bres|gico|gdau|qrcx|qrc|gdvideo)://[^"]+)")lit" );
        static QRegularExpression rx2( "'((?:bres|gico|gdau|qrcx|qrc|gdvideo)://[^']+)'" );
        set< QByteArray > resourceIncluded;
        vector< pair< QUrl, QString > > downloadResources;

        filterAndCollectResources( html, rx1, "\"", folder, resourceIncluded, downloadResources );
        filterAndCollectResources( html, rx2, "'", folder, resourceIncluded, downloadResources );

        int totalResources         = downloadResources.size();
        auto completedResourcesPtr = std::make_shared< int >( 0 );

        for ( const auto & p : downloadResources ) {
          ResourceToSaveHandler * handler = view_->saveResource( p.first, p.second, folder );
          if ( handler && !handler->isEmpty() ) {
            QObject::connect(
              handler,
              &ResourceToSaveHandler::done,
              this,
              [ this, totalResources, completedResourcesPtr ]() {
                *completedResourcesPtr += 1;
                if ( *completedResourcesPtr == totalResources ) {
                  // All resources completed
                  emit statusMessage( QObject::tr( "Save article complete" ), 5000 );
                }
                else {
                  emit statusMessage(
                    QObject::tr( "Saving article... (%1/%2)" ).arg( *completedResourcesPtr ).arg( totalResources ),
                    100 );
                }
              } );
          }
          else {
            // Handler is empty, count it as already completed
            *completedResourcesPtr += 1;
          }
        }

        file.write( html.toUtf8() );
      }
      else {
        file.write( html.toUtf8() );
        emit statusMessage( QObject::tr( "Save article complete" ), 5000 );
      }
    }
  } );
}
