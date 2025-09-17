/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QTextDocumentFragment>

#include <QRegularExpression>

#include "htmlescape.hh"

namespace Html {

string escape( const string & str )
{
  string result( str );

  for ( size_t x = result.size(); x--; ) {
    switch ( result[ x ] ) {
      case '&':
        result.erase( x, 1 );
        result.insert( x, "&amp;" );
        break;

      case '<':
        result.erase( x, 1 );
        result.insert( x, "&lt;" );
        break;

      case '>':
        result.erase( x, 1 );
        result.insert( x, "&gt;" );
        break;

      case '"':
        result.erase( x, 1 );
        result.insert( x, "&quot;" );
        break;

      default:
        break;
    }
  }

  return result;
}

static void storeLineInDiv( string & result, const string & line, bool baseRightToLeft )
{
  result += "<div";
  if ( unescape( QString::fromUtf8( line.c_str(), line.size() ) ).isRightToLeft() != baseRightToLeft ) {
    result += " dir=\"";
    result += baseRightToLeft ? "ltr\"" : "rtl\"";
  }
  result += ">";
  result += line + "</div>";
}

string preformat( const string & str, bool baseRightToLeft )
{
  string escaped = escape( str ), result, line;

  line.reserve( escaped.size() );
  result.reserve( escaped.size() );

  bool leading = true;

  for ( const char * nextChar = escaped.c_str(); *nextChar; ++nextChar ) {
    if ( leading ) {
      if ( *nextChar == ' ' ) {
        line += "&nbsp;";
        continue;
      }
      else if ( *nextChar == '\t' ) {
        line += "&nbsp;&nbsp;&nbsp;&nbsp;";
        continue;
      }
    }

    if ( *nextChar == '\n' ) {
      storeLineInDiv( result, line, baseRightToLeft );
      line.clear();
      leading = true;
      continue;
    }

    if ( *nextChar == '\r' ) {
      continue; // Just skip all \r
    }

    line.push_back( *nextChar );

    leading = false;
  }

  if ( !line.empty() ) {
    storeLineInDiv( result, line, baseRightToLeft );
  }

  return result;
}

string escapeForJavaScript( const string & str )
{
  string result( str );

  for ( size_t x = result.size(); x--; ) {
    switch ( result[ x ] ) {
      case '\\':
      case '"':
      case '\'':
        result.insert( x, 1, '\\' );
        break;

      case '\n':
        result.erase( x, 1 );
        result.insert( x, "\\n" );
        break;

      case '\r':
        result.erase( x, 1 );
        result.insert( x, "\\r" );
        break;

      case '\t':
        result.erase( x, 1 );
        result.insert( x, "\\t" );
        break;

      default:
        break;
    }
  }

  return result;
}

QString stripHtml( QString tmp )
{
  static QRegularExpression htmlRegex(
    "<(?:\\s*/?(?:div|h[1-6r]|q|p(?![alr])|br|li(?![ns])|td|blockquote|[uo]l|pre|d[dl]|nav|address))[^>]{0,}>",
    QRegularExpression::CaseInsensitiveOption );
  tmp.replace( htmlRegex, " " );
  static QRegularExpression htmlElementRegex( "<[^>]*>" );
  tmp.replace( htmlElementRegex, " " );
  return tmp;
}

QString unescape( QString str, HtmlOption option )
{
  // Does it contain HTML? If it does, we need to strip it
  if ( str.contains( '<' ) || str.contains( '&' ) ) {
    if ( option == HtmlOption::Strip ) {
      str = stripHtml( str );
    }
    return QTextDocumentFragment::fromHtml( str.trimmed() ).toPlainText();
  }
  return str;
}

QString fromHtmlEscaped( const QString & str )
{
  QString retVal = str;
  QRegularExpression regExp( R"((?<lt>\&lt\;)|(?<gt>\&gt\;)|(?<amp>\&amp\;)|(?<quot>\&quot\;))",
                             QRegularExpression::PatternOption::CaseInsensitiveOption );
  auto match = regExp.match( str, 0 );

  while ( match.hasMatch() ) {
    if ( !match.captured( "lt" ).isEmpty() ) {
      retVal.replace( match.capturedStart( "lt" ), match.capturedLength( "lt" ), "<" );
    }
    else if ( !match.captured( "gt" ).isEmpty() ) {
      retVal.replace( match.capturedStart( "gt" ), match.capturedLength( "gt" ), ">" );
    }
    else if ( !match.captured( "amp" ).isEmpty() ) {
      retVal.replace( match.capturedStart( "amp" ), match.capturedLength( "amp" ), "&" );
    }
    else if ( !match.captured( "quot" ).isEmpty() ) {
      retVal.replace( match.capturedStart( "quot" ), match.capturedLength( "quot" ), "\"" );
    }
    match = regExp.match( retVal, match.capturedStart() + 1 );
  }

  return retVal;
}

string unescapeUtf8( const string & str, HtmlOption option )
{
  return string( unescape( QString::fromStdString( str ), option ).toUtf8().data() );
}

QString escapeQString( const QString & str )
{
  QString result;
  result.reserve( str.size() * 6 ); // Reserve space for worst case scenario

  for ( const QChar & ch : str ) {
    switch ( ch.unicode() ) {
      case '&':
        result += "&amp;";
        break;
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '"':
        result += "&quot;";
        break;
      default:
        result += ch;
        break;
    }
  }

  return result;
}

} // namespace Html
