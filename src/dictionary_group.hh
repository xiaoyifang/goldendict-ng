/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef DICTIONARYGROUP_HH_INCLUDED
#define DICTIONARYGROUP_HH_INCLUDED

#include "audioplayerinterface.hh"
#include <memory>


class DictionaryGroup: public QObject
{
  Q_OBJECT

public:
  DictionaryGroup(QObject* parent,std::vector< sptr< Dictionary::Class > > const & allDictionaries_,
                          Instances::Groups const & groups_):allDictionaries(allDictionaries_),groups(groups_){

                          }
  ~DictionaryGroup();
  

  sptr< Dictionary::Class > getDictionaryByName(QString const & dictionaryName);

  std::vector< sptr< Dictionary::Class > > * const getActiveDictionaries(unsigned groupId);

  sptr< Dictionary::Class > getDictionaryById(std::string dictId);

  Instances::Group const * getGroupById(unsigned groupId);


private:
  std::vector< sptr< Dictionary::Class > > const & allDictionaries;
  Instances::Groups const & groups;
};

#endif // DICTIONARYGROUP_HH_INCLUDED
