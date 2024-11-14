#include <QRegularExpression>
#include "wildcard.hh"

/*
  Modified function from Qt
  Translates a wildcard pattern to an equivalent regular expression
  pattern (e.g., *.cpp to .*\.cpp).
*/

QString wildcardsToRegexp( const QString & wc_str )
{
  // The "anchored" version will enclose the output with \A...\z.
  return QRegularExpression::wildcardToRegularExpression( wc_str, QRegularExpression::UnanchoredWildcardConversion );
}
