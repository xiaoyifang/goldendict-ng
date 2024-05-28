#include "globalregex.hh"
#include "fulltextsearch.hh"

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
QRegularExpression Mdx::allLinksRe( R"((?:<\s*(a(?:rea)?|img|link|script|source|audio|video)(?:\s+[^>]+|\s*)>))",
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
                                       QRegularExpression::CaseInsensitiveOption
                                         | QRegularExpression::InvertedGreedinessOption );
const QRegularExpression Mdx::stylesRe(
  R"(([\s"']href\s*=)\s*(["'])(?!\s*\b(?:(?:bres|https?|ftp)://|(?:data|javascript):))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^">]+)\2)",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::stylesRe2(
  R"(([\s"']href\s*=)\s*(?![\s"']|\b(?:(?:bres|https?|ftp)://|(?:data|javascript):))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^\s">]+))",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::inlineScriptRe( R"(<\s*script(?:(?=\s)(?:(?![\s"']src\s*=)[^>])+|\s*)>)",
                                        QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::closeScriptTagRe( R"(<\s*/script\s*>)", QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::srcRe(
  R"(([\s"'](?:src|srcset)\s*=)\s*(["'])(?!\s*\b(?:(?:bres|https?|ftp)://|(?:data|javascript):))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^">]+)\2)",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::srcRe2(
  R"(([\s"'](?:src|srcset)\s*=)\s*(?![\s"']|\b(?:(?:bres|https?|ftp)://|(?:data|javascript):))(?:file://)?[\x00-\x1f\x7f]*\.*/?([^\s">]+))",
  QRegularExpression::CaseInsensitiveOption );
QRegularExpression Mdx::srcSetRe( R"(([\s\"'](?:srcset)\s*=)\s*([\"'])[\x00-\x1f\x7f]*\.*/?([^\">]+)\2)",
                                  QRegularExpression::CaseInsensitiveOption );

QRegularExpression Mdx::links( R"(url\(\s*(['"]?)([^'"]*)(['"]?)\s*\))", QRegularExpression::CaseInsensitiveOption );

QRegularExpression Mdx::fontFace( R"((?:url\s*\(\s*\"(.*?)\"\s*)\))",
                                  QRegularExpression::CaseInsensitiveOption
                                    | QRegularExpression::DotMatchesEverythingOption );

QRegularExpression Mdx::styleElement( R"((<style[^>]*>)([\w\W]*?)(<\/style>))",
                                      QRegularExpression::CaseInsensitiveOption );


QRegularExpression Epwing::refWord( R"([r|p](\d+)at(\d+))", QRegularExpression::CaseInsensitiveOption );


bool Html::containHtmlEntity( std::string const & text )
{
  return QString::fromStdString( text ).contains( htmlEntity );
}