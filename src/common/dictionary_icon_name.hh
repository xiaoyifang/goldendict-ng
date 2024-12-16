#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace Icons {
using namespace std;
//use dictionary name's (first character + the order number) represent the dictionary name
class DictionaryIconName
{
  //map icon name to dictionary names;
  map< string, vector< string > > _iconDictionaryNames;
  //map dictionary name to icon name;
  map<string,string> _dictionaryIconNames;

  std::mutex _mutex;

public:
  std::string getIconName( const std::string & dictionaryName ) const;
}
} // namespace Icons