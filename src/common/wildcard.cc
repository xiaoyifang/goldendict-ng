#include <QRegularExpression>
#include "wildcard.hh"

/*
  Modified function from Qt
  Translates a wildcard pattern to an equivalent regular expression
  pattern (e.g., *.cpp to .*\.cpp).
*/

QString wildcardsToRegexp( const QString & wc_str )
{
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
  //qt 5.X does not offer an unanchored version. the default output is enclosed between \A   \z.
  auto anchorPattern = QRegularExpression::wildcardToRegularExpression( wc_str );
  return anchorPattern.mid( 2, anchorPattern.length() - 4 );
#else
  return QRegularExpression::wildcardToRegularExpression( wc_str, QRegularExpression::UnanchoredWildcardConversion );
#endif
}
