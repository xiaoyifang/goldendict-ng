/* Licensed under GPLv3 or later, see the LICENSE file */

#ifndef DICTIONARYGROUP_HH_INCLUDED
#define DICTIONARYGROUP_HH_INCLUDED

#include "sptr.hh"
#include <QObject>
#include "dictionary.hh"
#include "instances.hh"

class DictionaryGroup: public QObject
{
  Q_OBJECT

public:
  DictionaryGroup( QObject * parent,
                   std::vector< sptr< Dictionary::Class > > const & allDictionaries_,
                   Instances::Groups const & groups_ ):
    QObject( parent ),
    allDictionaries( allDictionaries_ ),
    groups( groups_ )
  {
  }

  sptr< Dictionary::Class > getDictionaryByName( QString const & dictionaryName );

  const std::vector< sptr< Dictionary::Class > > * getActiveDictionaries( unsigned groupId );

  sptr< Dictionary::Class > getDictionaryById( const std::string & dictId );

  Instances::Group const * getGroupById( unsigned groupId );


private:
  std::vector< sptr< Dictionary::Class > > const & allDictionaries;
  Instances::Groups const & groups;
};

#endif // DICTIONARYGROUP_HH_INCLUDED
