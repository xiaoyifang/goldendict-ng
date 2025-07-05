#pragma once

#include <QByteArray>
#include <QMap>

namespace Epwing {

class EpwingCharmap
{
public:
  /// The class is a singleton.
  static EpwingCharmap & instance();

  /// Map Epwing extra char to Utf-8
  QByteArray mapToUtf8( const QString & code );

private:
  void addEntry( const QString & code, int ch );

  QMap< QString, QChar > charMap;
};

} // namespace Epwing
