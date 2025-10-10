#include "utils.hh"
#include <QDir>
#include <QMessageBox>
#include <string>
#include <QBuffer>
#include <QMimeDatabase>
#include <QGuiApplication>

using std::string;
namespace Utils {

bool isWayland()
{
    // QGuiApplication::platformName() is the most reliable way for a Qt application
    // to check the windowing system it is running on.
    // It returns "wayland" or "wayland-egl" for Wayland sessions.
    // It returns "xcb" for X11 sessions.
    return QGuiApplication::platformName().startsWith( "wayland", Qt::CaseInsensitive );
}

QMap< QString, QString > str2map( const QString & contextsEncoded )
{
  QMap< QString, QString > contexts;

  if ( contextsEncoded.size() ) {
    QByteArray ba = QByteArray::fromBase64( contextsEncoded.toLatin1() );
    QBuffer buf( &ba );
    buf.open( QBuffer::ReadOnly );
    QDataStream stream( &buf );
    stream >> contexts;
  }
  return contexts;
}
//some str has \0 in the middle of the string. return the string before the \0
std::string c_string( const QString & str )
{
  return std::string( str.toUtf8().constData() );
}

bool endsWithIgnoreCase( QByteArrayView str, QByteArrayView extension )
{
  return ( str.size() >= extension.size() )
    && ( str.last( extension.size() ).compare( extension, Qt::CaseInsensitive ) == 0 );
}

QString escapeAmps( const QString & str )
{
  QString result( str );
  result.replace( "&", "&&" );
  return result;
}

QString unescapeAmps( const QString & str )
{
  QString result( str );
  result.replace( "&&", "&" );
  return result;
}
} // namespace Utils

QString Utils::Path::combine( const QString & path1, const QString & path2 )
{
  return QDir::cleanPath( path1 + QDir::separator() + path2 );
}

QString Utils::Url::getSchemeAndHost( const QUrl & url )
{
  if ( !url.isValid() ) {
    return QString();
  }

  QString scheme = url.scheme(); // http or https
  QString host   = url.host();   // example.com
  auto port      = url.port( -1 );

  QString origin = scheme + "://" + host;

  if ( port != -1 ) {
    if ( ( scheme == "http" && port != 80 ) || ( scheme == "https" && port != 443 ) ) {
      origin += ":" + QString::number( port );
    }
  }

  return origin;
}

QString Utils::Url::extractBaseDomain( const QString & domain )
{
  // More generic approach for detecting two-part TLDs
  // Look for patterns like com.xx, co.xx, org.xx, gov.xx, net.xx, edu.xx
  QStringList parts = domain.split( '.' );
  if ( parts.size() >= 3 ) {
    QString secondLevel = parts[ parts.size() - 2 ];
    QString topLevel    = parts[ parts.size() - 1 ];

    // Check if the second level is a common second-level domain indicator
    // and the top level is a standard TLD (2-3 characters)
    if ( ( secondLevel == "com" || secondLevel == "co" || secondLevel == "org" || secondLevel == "gov"
           || secondLevel == "net" || secondLevel == "edu" )
         && ( topLevel.length() == 2 || topLevel.length() == 3 ) ) {
      // Extract the registrable domain (e.g., "example.com" from "www.example.com.jp")
      if ( parts.size() >= 3 ) {
        return parts[ parts.size() - 3 ] + "." + secondLevel;
      }
      return secondLevel + "." + topLevel;
    }
  }

  // For domains with multiple parts, extract the last two parts as base domain
  int dotCount = domain.count( '.' );
  if ( dotCount >= 2 ) {
    if ( parts.isEmpty() ) {
      parts = domain.split( '.' );
    }
    if ( parts.size() >= 2 ) {
      return parts[ parts.size() - 2 ] + "." + parts[ parts.size() - 1 ];
    }
  }

  // For domains with one or no dots, return as is
  return domain;
}

void Utils::Widget::setNoResultColor( QWidget * widget, bool noResult )
{
  if ( noResult ) {
    auto font = widget->font();
    font.setItalic( true );

    widget->setFont( font );
  }
  else {
    auto font = widget->font();
    font.setItalic( false );

    widget->setFont( font );
  }
}

std::string Utils::Html::getHtmlCleaner()
{
  return R"(</font></font></font></font></font></font></font></font></font></font></font></font>
                     </b></b></b></b></b></b></b></b>
                     </i></i></i></i></i></i></i></i>
                     </a></a></a></a></a></a></a></a>)";
}


namespace Utils::Fs {

char separator()
{
  return QDir::separator().toLatin1();
}

std::string basename( const std::string & str )
{
  size_t x = str.rfind( separator() );

  if ( x == std::string::npos ) {
    return str;
  }

  return std::string( str, x + 1 );
}

void removeDirectory( const QString & directory )
{
  QDir dir( directory );
  dir.removeRecursively();
}

void removeDirectory( const string & directory )
{
  removeDirectory( QString::fromStdString( directory ) );
}

} // namespace Utils::Fs

namespace Utils::WebSite {
QString urlReplaceWord( const QString url, QString inputWord )
{
  //copy temp url
  auto urlString = url;

  urlString.replace( "%25GDWORD%25", inputWord.toUtf8().toPercentEncoding() );

  return urlString;
}
} // namespace Utils::WebSite

QString Utils::getAudioMimeType( const QString & path, QString & extension )
{
  // Default values
  extension        = ".wav";
  QString mimeType = "audio/wav";

  // Use QMimeDatabase to determine MIME type based on file extension
  QMimeDatabase mimeDb;
  QFileInfo fileInfo( path );
  if ( !fileInfo.suffix().isEmpty() ) {
    extension      = "." + fileInfo.suffix().toLower();
    QMimeType mime = mimeDb.mimeTypeForFile( fileInfo, QMimeDatabase::MatchExtension );
    if ( mime.isValid() && mime.name().startsWith( "audio/" ) ) {
      mimeType = mime.name();
    }
  }

  return mimeType;
}
