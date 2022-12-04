#include "utils.hh"
#include <QDir>
#include <QTextDocumentFragment>

QString Utils::trim( const QString & str ,const QRegularExpression & reg)
{
  QString remain;
  int n = str.size() - 1;
  for( ; n >= 0; --n )
  {
    auto c = str.at( n );
    auto match= reg.match(QString()+c);
    if( !match.hasMatch() )
    {
      remain = str.left( n + 1 );
      break;
    }
  }

  n = 0;
  for( ; n < remain.size(); n++ )
  {
    auto c = remain.at( n );
    auto match= reg.match(QString()+c);
    if( !match.hasMatch() )
    {
      return remain.mid( n );
    }
  }

  return "";
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

//copy from HtmlEscaple.unescaple.
QString Utils::Html::toPlainText( QString const & str)
{
  // Does it contain HTML? If it does, we need to strip it
  if ( str.contains( '<' ) || str.contains( '&' ) )
  {
    QString tmp = str;
    {
      tmp.replace( QRegularExpression( "<(?:\\s*/?(?:div|h[1-6r]|q|p(?![alr])|br|li(?![ns])|td|blockquote|[uo]l|pre|d[dl]|nav|address))[^>]{0,}>",
                     QRegularExpression::CaseInsensitiveOption ), " " );
      tmp.replace( QRegularExpression( "<[^>]*>"), " ");

    }
    return QTextDocumentFragment::fromHtml( tmp.trimmed() ).toPlainText();
  }
  return str;
}
