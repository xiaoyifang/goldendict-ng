#ifndef __EPWING_CHARMAP_HH_INCLUDED_
#define __EPWING_CHARMAP_HH_INCLUDED_

#include <QByteArray>
#include <QMap>

namespace Epwing {

class EpwingCharmap
{
public:
  /// The class is a singleton.
  static EpwingCharmap & instance();

  /// Map Epwing extra char to Utf-8
  QByteArray mapToUtf8( QString const & code );

private:
  void addEntry( QString const & code, int ch );

  QMap< QString, QChar > charMap;
};

} // namespace Epwing

#endif // __EPWING_CHARMAP_HH_INCLUDED_
