#pragma once

#include <QRegularExpression>

namespace RX {
class Ftx
{
public:
  static QRegularExpression regBrackets;
  static QRegularExpression regSplit;
  static QRegularExpression spacesRegExp;
  static QRegularExpression wordRegExp;
  static QRegularExpression setsRegExp;
  static QRegularExpression regexRegExp;
  static QRegularExpression handleRoundBracket;
  static QRegularExpression noRoundBracket;

  static QRegularExpression tokenBoundary;

  static QRegularExpression token;
};


class Mdx
{
public:
  static QRegularExpression allLinksRe;
  static QRegularExpression wordCrossLink;
  static QRegularExpression anchorIdRe;
  static QRegularExpression anchorIdReWord;
  static QRegularExpression anchorIdRe2;
  static QRegularExpression anchorLinkRe;
  const static QRegularExpression audioRe;
  const static QRegularExpression stylesRe;
  static QRegularExpression stylesRe2;
  static QRegularExpression inlineScriptRe;
  static QRegularExpression closeScriptTagRe;
  static QRegularExpression srcRe;
  static QRegularExpression srcRe2;
  static QRegularExpression srcset;
  static QRegularExpression objectdata;

  static QRegularExpression links;
  static QRegularExpression fontFace;
  static QRegularExpression styleElement;
};

namespace Zim {
//leading dot slash namespace
const static QRegularExpression leadingDotSlash( R"(^\.{0,2}\/)" );
} // namespace Zim

class Epwing
{
public:
  static QRegularExpression refWord;
};

namespace Html {
const static QRegularExpression startDivTag( R"(<div[\s>])" );
const static QRegularExpression htmlEntity( R"(&(?:#\d+|#[xX][\da-fA-F]+|[0-9a-zA-Z]+);)" );


// exclude <br/> <hr/>
const static QRegularExpression emptyXmlTag( R"(<(?!(br|hr)\b)([^/ >]*)\s*/>)" );

bool containHtmlEntity( const std::string & text );
} // namespace Html

const static QRegularExpression accentMark( R"(\p{M})", QRegularExpression::UseUnicodePropertiesOption );
//contain unicode space mark,invisible, and punctuation
const static QRegularExpression markPuncSpace( R"([\p{M}\p{Z}\p{C}\p{P}])",
                                               QRegularExpression::UseUnicodePropertiesOption );
//contain unicode space and mark.
const static QRegularExpression markSpace( R"([\p{M}\p{Z}])", QRegularExpression::UseUnicodePropertiesOption );

const static QRegularExpression whiteSpace( "\\s+" );

} // namespace RX
