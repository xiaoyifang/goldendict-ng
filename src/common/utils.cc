#include "utils.hh"
#include <QDir>
#include <QPalette>
#include <QStyle>
#include <QMessageBox>
#include <string>
#ifdef _MSC_VER
  #include <stub_msvc.h>
#endif
using std::string;
namespace Utils {
//some str has \0 in the middle of the string. return the string before the \0
std::string c_string( const QString & str )
{
  return std::string( str.toUtf8().constData() );
}

bool endsWithIgnoreCase( const string & str1, string str2 )
{
  return ( str1.size() >= (unsigned)str2.size() )
    && ( strcasecmp( str1.c_str() + ( str1.size() - str2.size() ), str2.data() ) == 0 );
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
    font.setItalic(true);

    widget->setFont( font );
  }
  else {
    auto font = widget->font();
    font.setItalic(false);

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

  if ( x == std::string::npos )
    return str;

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
