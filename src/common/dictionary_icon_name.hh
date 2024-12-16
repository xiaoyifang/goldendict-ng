#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <QString>

namespace Icons {
using namespace std;
//use dictionary name's (first character + the order number) represent the dictionary name
class DictionaryIconName
{
  //map icon name to dictionary names;
  map< QString, vector< QString > > _iconDictionaryNames;
  //map dictionary name to icon name;
  map<QString,QString> _dictionaryIconNames;

  std::mutex _mutex;

public:
  QString getIconName( const QString & dictionaryName ) const;
}
} // namespace Icons