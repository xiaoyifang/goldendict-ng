/* Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "sptr.hh"
#include "dict/dictionary.hh"
#include "instances.hh"

class DictionaryGroup
{
public:
  DictionaryGroup( const std::vector< sptr< Dictionary::Class > > & allDictionaries_,
                   const Instances::Groups & groups_ ):
    allDictionaries( allDictionaries_ ),
    groups( groups_ )
  {
  }

  sptr< Dictionary::Class > getDictionaryByName( const QString & dictionaryName );

  const std::vector< sptr< Dictionary::Class > > * getActiveDictionaries( unsigned groupId );

  sptr< Dictionary::Class > getDictionaryById( const std::string & dictId );

  const Instances::Group * getGroupById( unsigned groupId );


private:
  const std::vector< sptr< Dictionary::Class > > & allDictionaries;
  const Instances::Groups & groups;
};
