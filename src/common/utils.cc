#include "utils.hh"
#include <QDir>
#include <QPalette>
#include <QStyle>
#include <QMessageBox>
#include <string>
#include <QBuffer>

using std::string;
namespace Utils {
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

QString escapeAmps( QString const & str )
{
  QString result( str );
  result.replace( "&", "&&" );
  return result;
}

QString unescapeAmps( QString const & str )
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

QString Utils::Url::getSchemeAndHost( QUrl const & url )
{
  auto _url         = url.url();
  auto index        = _url.indexOf( "://" );
  auto hostEndIndex = _url.indexOf( "/", index + 3 );
  return _url.mid( 0, hostEndIndex );
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

std::string basename( std::string const & str )
{
  size_t x = str.rfind( separator() );

  if ( x == std::string::npos ) {
    return str;
  }

  return std::string( str, x + 1 );
}

void removeDirectory( QString const & directory )
{
  QDir dir( directory );
  dir.removeRecursively();
}

void removeDirectory( string const & directory )
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
