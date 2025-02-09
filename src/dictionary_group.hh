/* Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "sptr.hh"
#include "dict/dictionary.hh"
#include "group.hh"

class DictionaryGroup
{
public:
  DictionaryGroup( std::vector< sptr< Dictionary::Class > > const & allDictionaries_, GroupInstances const & groups_ ):
    allDictionaries( allDictionaries_ ),
    groups( groups_ )
  {
  }

  sptr< Dictionary::Class > getDictionaryByName( QString const & dictionaryName );

  const std::vector< sptr< Dictionary::Class > > * getActiveDictionaries( unsigned groupId );

  sptr< Dictionary::Class > getDictionaryById( const std::string & dictId );

  Group const * getGroupById( unsigned groupId );


private:
  std::vector< sptr< Dictionary::Class > > const & allDictionaries;
  GroupInstances const & groups;
};
