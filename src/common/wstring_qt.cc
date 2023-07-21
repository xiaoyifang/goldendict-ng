#include "wstring_qt.hh"
#include <QVector>

namespace gd {
wstring toWString( QString const & in )
{
  return in.toStdU32String();
}

// When convert non-BMP characters to wstring,the ending char maybe \0 .This method remove the tailing \0 from the wstring
// as \0 is sensitive in the index.  This method will be only used with index related operations like store/query.
wstring removeTrailingZero( wstring const & v )
{
  int n = v.size();
  while ( n > 0 && v[ n - 1 ] == 0 )
    n--;
  return wstring( v.data(), n );
}

wstring removeTrailingZero( QString const & in )
{
  QVector< unsigned int > v = in.toUcs4();

  int n = v.size();
  while ( n > 0 && v[ n - 1 ] == 0 )
    n--;
  if ( n != v.size() )
    v.resize( n );

  return wstring( (const wchar *)v.constData(), v.size() );
}

wstring normalize( const wstring & str )
{
  return toWString( QString::fromStdU32String( str ).normalized( QString::NormalizationForm_C ) );
}


} // namespace gd
