#pragma once

#include <string>
#include <QMap>
#include <vector>
#include <mutex>
#include <QString>
#include <QMutex>

namespace Icons {
//use dictionary name's (first character + the order number) to represent the dictionary name in the icon image.
class DictionaryIconName
{
  //map icon name to dictionary names;
  QMap< QString, int > _iconDictionaryNames;
  //map dictionary name to icon name;
  QMap< QString, QString > _dictionaryIconNames;

  QMutex _mutex;

public:
  QString getIconName( const QString & dictionaryName );
};
} // namespace Icons