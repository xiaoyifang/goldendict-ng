#ifndef GLOBALREGEX_HH
#define GLOBALREGEX_HH

#include <QRegularExpression>

namespace RX
{
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
  static QRegularExpression audioRe;
  static QRegularExpression stylesRe;
  static QRegularExpression stylesRe2;
  static QRegularExpression inlineScriptRe;
  static QRegularExpression closeScriptTagRe;
  static QRegularExpression srcRe;
  static QRegularExpression srcRe2;

  static QRegularExpression links;
  static QRegularExpression fontFace;
  static QRegularExpression styleElment;
};

class Zim{
 public:
  static QRegularExpression linkSpecialChar;
};

class Epwing{
 public:
  static QRegularExpression refWord;
};

namespace Html {
const static QRegularExpression startDivTag( R"(<div[\s>])" );
const static QRegularExpression htmlEntity( R"(&(?:#\d+|#[xX][\da-fA-F]+|[0-9a-zA-Z]+);)" );

bool containHtmlEntity( std::string const & text );
}

} // namespace RX

#endif // GLOBALREGEX_HH
