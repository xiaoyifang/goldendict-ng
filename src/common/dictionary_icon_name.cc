#include "dictionary_icon_name.hh"


std::string Icons::DictionaryIconName::getIconName( const std::string & dictionaryName ) const
{
    if (dictionaryName.empty()) {
        return {};
    }
    std::lock_guard<std::mutex> lock(_mutex);

    auto it = _dictionaryIconNames.find(dictionaryName);
    if (it != _dictionaryIconNames.end()) {
        return it->second;
    } else {
        //get the first character of the dictionary name
        std::string name = dictionaryName.substr(0,1);
        auto it1         = _iconDictionaryNames.find( name );
        if (it1 != _iconDictionaryNames.end()) {
            it1->second.push_back( dictionaryName );
        }

        name= name+ it1->second.size();
        return _dictionaryIconNames[dictionaryName] = name;
    }
}