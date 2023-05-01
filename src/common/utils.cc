#include "utils.hh"
#include <QDir>
#include <QPalette>
#include <QStyle>
#include <QMessageBox>

namespace Utils {
//some str has \0 in the middle of the string. return the string before the \0
std::string c_string( const QString & str )
{
  return std::string( str.toUtf8().constData() );
}
}

QString Utils::Path::combine(const QString& path1, const QString& path2)
{
  return QDir::cleanPath(path1 + QDir::separator() + path2);
}

QString Utils::Url::getSchemeAndHost( QUrl const & url )
{
  auto _url = url.url();
  auto index = _url.indexOf("://");
  auto hostEndIndex = _url.indexOf("/",index+3);
  return _url.mid(0,hostEndIndex);
}

void Utils::Widget::setNoResultColor(QWidget * widget, bool noResult)
{
  if( noResult ) {
    QPalette pal( widget->palette() );
    //    #febb7d
    QRgb rgb = 0xfebb7d;
    pal.setColor( QPalette::Base, QColor( rgb ) );
    widget->setAutoFillBackground( true );
    widget->setPalette( pal );
  }
  else {
    QPalette pal( widget->style()->standardPalette() );
    widget->setAutoFillBackground( true );
    widget->setPalette( pal );
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

} // namespace FsEncoding
