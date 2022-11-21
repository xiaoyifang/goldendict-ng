#include "utils.hh"
#include <QDir>

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
