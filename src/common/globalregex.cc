#include "globalregex.hh"
#include "fulltextsearch.hh"
#include <QJsonArray>
#include <QJsonDocument>

using namespace RX;

QRegularExpression
  Ftx::regBrackets( R"((\([\w\p{M}]+\)){0,1}([\w\p{M}]+)(\([\w\p{M}]+\)){0,1}([\w\p{M}]+){0,1}(\([\w\p{M}]+\)){0,1})",
                    QRegularExpression::UseUnicodePropertiesOption );
QRegularExpression Ftx::regSplit( "[^\\w\\p{M}]+", QRegularExpression::UseUnicodePropertiesOption );

QRegularExpression Ftx::spacesRegExp( "\\W+", QRegularExpression::UseUnicodePropertiesOption );
QRegularExpression Ftx::wordRegExp( QString( "\\w{" ) + QString::number( FTS::MinimumWordSize ) + ",}",
                                    QRegularExpression::UseUnicodePropertiesOption );
QRegularExpression Ftx::setsRegExp( R"(\[[^\]]+\])", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Ftx::regexRegExp( R"(\\[afnrtvdDwWsSbB]|\\x([0-9A-Fa-f]{4})|\\0([0-7]{3}))",
                                     QRegularExpression::CaseInsensitiveOption );

QRegularExpression Ftx::handleRoundBracket( R"([^\w\(\)\p{M}]+)", QRegularExpression::UseUnicodePropertiesOption );
QRegularExpression Ftx::noRoundBracket( R"([^\w\p{M}]+)", QRegularExpression::UseUnicodePropertiesOption );

QRegularExpression Ftx::tokenBoundary( R"([\*\?\+]|\bAnd\b|\bOR\b)", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Ftx::token( R"((".*?")|([\w\W\+\-]+))",
                               QRegularExpression::DotMatchesEverythingOption
                                 | QRegularExpression::CaseInsensitiveOption );
//mdx

//<audio src="xxx"> is also a valid url.
QRegularExpression Mdx::allLinksRe( R"((?:<\s*(a(?:rea)?|img|link|script|source|audio|video|object)(?:\s+[^>]+|\s*)>))",
                                    QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::wordCrossLink( R"(([\s"']href\s*=)\s*(["'])entry://([^>#]*?)((?:#[^>]*?)?)\2)",
                                       QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::anchorIdRe( R"(([\s"'](?:name|id)\s*=)\s*(["'])\s*(?=\S))",
                                    QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::anchorIdReWord( R"(([\s"'](?:name|id)\s*=)\s*(["'])\s*(?=\S)([^"]*))",
                                        QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::anchorIdRe2( R"(([\s"'](?:name|id)\s*=)\s*(?=[^"'])([^\s">]+))",
                                     QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::anchorLinkRe( R"(([\s"']href\s*=\s*["'])entry://#)",
                                      QRegularExpression::CaseInsensitiveOption );
const QRegularExpression Mdx::audioRe( R"(([\s"']href\s*=)\s*(["'])sound://([^">]+)\2)",
                                       QRegularExpression::CaseInsensitiveOption );
const QRegularExpression Mdx::stylesRe(
  R"(([\s"']href\s*=)\s*(["'])(?!\s*(?:\b(?:bres|https?|ftp)://|(?:data|javascript):|//))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^">]+)\2)",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::stylesRe2(
  R"(([\s"']href\s*=)\s*(?![\s"']|\b(?:(?:bres|https?|ftp)://|(?:data|javascript):)|//)(?:file://)?[\x00-\x1f\x7f]*\.*/?([^\s">]+))",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::inlineScriptRe( R"(<\s*script(?:(?=\s)(?:(?![\s"']src\s*=)[^>])+|\s*)>)",
                                        QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::closeScriptTagRe( R"(<\s*/script\s*>)", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::srcRe(
  R"(([\s"']src\s*=)\s*(["'])(?!\s*(?:\b(?:bres|https?|ftp)://|(?:data|javascript):|//))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^">]+)\2)",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::srcRe2(
  R"(([\s"']src\s*=)\s*(?![\s"']|\b(?:(?:bres|https?|ftp)://|(?:data|javascript):)|//)(?:file://)?[\x00-\x1f\x7f]*\.*/?([^\s">]+))",
  QRegularExpression::CaseInsensitiveOption );
// matches srcset in <img srcset="text">
QRegularExpression Mdx::srcset( R"((?<before>[\s]srcset\s*=\s*(?<q>["']))(?<text>[^">]*?)(?<after>\k<q>))",
                                QRegularExpression::CaseInsensitiveOption );

// matches data in <object data="text">
QRegularExpression Mdx::objectdata( R"((?<before>[\s]data\s*=\s*(?<q>["']))(?<text>[^">]*?)(?<after>\k<q>))",
                                    QRegularExpression::CaseInsensitiveOption );

QRegularExpression Mdx::links( R"(url\(\s*(['"]?)([^'"]*)(['"]?)\s*\))", QRegularExpression::CaseInsensitiveOption );

QRegularExpression Mdx::fontFace( R"((?:url\s*\(\s*\"(.*?)\"\s*)\))",
                                  QRegularExpression::CaseInsensitiveOption
                                    | QRegularExpression::DotMatchesEverythingOption );

QRegularExpression Mdx::styleElement( R"((<style[^>]*>)([\w\W]*?)(<\/style>))",
                                      QRegularExpression::CaseInsensitiveOption );

QRegularExpression Mdx::protocolRelativeUrlQuoted( R"(([\s"'](?:src|href|data)\s*=\s*["'])\/\/)" );
QRegularExpression Mdx::protocolRelativeUrlUnquoted( R"(([\s"'](?:src|href|data)\s*=\s*)(?!\s*["'])\/\/)" );
QRegularExpression Mdx::htmlTagStart( "<html\\b", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::htmlTagEnd( "</html>", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::bodyTagStart( "<body\\b", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::bodyTagEnd( "</body>", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::headTagStart( "<head\\b", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::headTagEnd( "</head>", QRegularExpression::CaseInsensitiveOption );

QRegularExpression Epwing::refWord( R"([r|p](\d+)at(\d+))", QRegularExpression::CaseInsensitiveOption );

const QRegularExpression RX::qtWebEngineUserAgent( R"(QtWebEngine\/[\d.]+\s*)" );
const QRegularExpression RX::windowsNtVersion( R"(Windows NT [\d.]+)" );

bool Html::containHtmlEntity( const std::string & text )
{
  return QString::fromStdString( text ).contains( htmlEntity );
}

QStringList RX::Ftx::processSearchStringForHighlight( const QString & searchString )
{
  QStringList highlightKeywords;

  if ( searchString.isEmpty() ) {
    return highlightKeywords;
  }

  int pos    = 0;
  int length = searchString.length();
  static QRegularExpression quotedPhraseRx( R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')" );
  static QRegularExpression xapianOpsRx( R"(\bAND\b|\bOR\b|[\+\-\*])" );

  while ( pos < length ) {
    QRegularExpressionMatch match = quotedPhraseRx.match( searchString, pos );

    if ( match.hasMatch() && match.capturedStart() == pos ) {
      QString phrase = match.captured();
      phrase         = phrase.mid( 1, phrase.length() - 2 );

      // let mark.js handle exact matching with accuracy option
      highlightKeywords.append( phrase );
      pos = match.capturedEnd();
    }
    else if ( searchString[ pos ].isSpace() ) {
      ++pos;
    }
    else {
      QString token;
      while ( pos < length && !searchString[ pos ].isSpace() && searchString[ pos ] != '"'
              && searchString[ pos ] != '\'' ) {
        token += searchString[ pos ];
        ++pos;
      }

      if ( !token.isEmpty() && !token.startsWith( '-' ) ) {
        token.replace( xapianOpsRx, " " );
        token = token.simplified();

        if ( !token.isEmpty() ) {
          highlightKeywords.append( token );
        }
      }
    }
  }

  return highlightKeywords;
}

QString RX::Ftx::serializeKeywordsToJson( const QStringList & keywords )
{
  QJsonArray jsonArray;
  for ( const QString & keyword : keywords ) {
    jsonArray.append( keyword );
  }
  QJsonDocument jsonDoc( jsonArray );
  return QString::fromUtf8( jsonDoc.toJson( QJsonDocument::Compact ) );
}
