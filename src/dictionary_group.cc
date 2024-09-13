/* Licensed under GPLv3 or later, see the LICENSE file */


#include "dictionary_group.hh"


sptr< Dictionary::Class > DictionaryGroup::getDictionaryByName( QString const & dictName )
{
  // Link to other dictionary
  for ( const auto & allDictionarie : allDictionaries ) {
    if ( dictName.compare( QString::fromUtf8( allDictionarie->getName().c_str() ) ) == 0 ) {
      return allDictionarie;
    }
  }
  return nullptr;
}

const std::vector< sptr< Dictionary::Class > > * DictionaryGroup::getActiveDictionaries( unsigned currentGroup )
{
  std::vector< sptr< Dictionary::Class > > const * activeDicts = nullptr;

  if ( !groups.empty() ) {
    for ( const auto & group : groups )
      if ( group.id == currentGroup ) {
        activeDicts = &( group.dictionaries );
        break;
      }
  }
  else
    activeDicts = &allDictionaries;

  return activeDicts;
}


sptr< Dictionary::Class > DictionaryGroup::getDictionaryById( const std::string & dictId )
{

  for ( unsigned x = allDictionaries.size(); x--; ) {
    if ( allDictionaries[ x ]->getId() == dictId ) {
      return allDictionaries[ x ];
    }
  }
  return nullptr;
}

Instances::Group const * DictionaryGroup::getGroupById( unsigned groupId )
{
  return groups.findGroup( groupId );
}
