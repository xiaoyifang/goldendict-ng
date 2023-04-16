#include "wstring_qt.hh"
#include <QVector>

namespace gd
{

  //This is not only about non-BMP characters.even without non-BMP. this wrapper has removed the tailing \0
  //so even https://bugreports.qt-project.org/browse/QTBUG-25536 has been fixed . It can not directly be replaced by
  // QString::toStd32String();
  wstring toWString( QString const & in )
  {
    QVector< unsigned int > v = in.toUcs4();

    // Fix for QString instance which contains non-BMP characters
    // Qt will created unexpected null characters may confuse btree indexer.
    // Related: https://bugreports.qt-project.org/browse/QTBUG-25536
    int n = v.size();
    while ( n > 0 && v[ n - 1 ] == 0 ) n--;
    if ( n != v.size() )
      v.resize( n );

    return wstring( ( const wchar * ) v.constData(), v.size() );
  }

  wstring normalize( const wstring & str )
  {
    return gd::toWString( QString::fromStdU32String( str ).normalized( QString::NormalizationForm_C ) );
  }



}
