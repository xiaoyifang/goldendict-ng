/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "config.hh"
#include "dict/dictionary.hh"
#include <QIcon>
#include <limits.h>

// Note that a group hold references to dictionaries directly instead of only having their ids

struct Group
{
  unsigned id;
  QString name, icon, favoritesFolder;
  QIcon iconData;
  QKeySequence shortcut;
  std::vector< sptr< Dictionary::Class > > dictionaries;

  /// Instantiates the given group from its configuration. If some dictionary
  /// wasn't found, it just skips it.
  Group( Config::Group const & cfgGroup,
         std::vector< sptr< Dictionary::Class > > const & allDictionaries,
         Config::Group const & inactiveGroup );

  /// Creates an empty group.
  explicit Group();

  Group( unsigned id, QString name_ );

  /// Makes the configuration group from the current contents.
  Config::Group makeConfigGroup();

  /// Makes an icon object for this group, based on the icon's name or iconData.
  QIcon makeIcon() const;

  /// Remove id's if not presented in group dictionaries
  void checkMutedDictionaries( Config::MutedDictionaries * mutedDictionaries ) const;

  /// Adds any dictionaries not already present in the given group or in
  /// inactiveDictionaires to its end. Meant to be used with dictionaryOrder
  /// special group.
  static void complementDictionaryOrder( Group & dictionaryOrder,
                                  Group const & inactiveDictionaries,
                                  std::vector< sptr< Dictionary::Class > > const & allDictionaries );

  /// For any dictionaries present in the group, updates their names to match
  /// the dictionaries they refer to in their current form, if they exist.
  /// If the dictionary instance can't be located, the name is left untouched.
  static void updateNames( Config::Group &, std::vector< sptr< Dictionary::Class > > const & allDictionaries );

  /// Does updateNames() for any relevant dictionary groups present in the
  /// configuration.
  static void updateNames( Config::Class &, std::vector< sptr< Dictionary::Class > > const & allDictionaries );

  /// Creates icon from icon data. Used by Group, but also by others who work
  /// with icon data directly.
  static QIcon iconFromData( QByteArray const & );
};

struct GroupInstances: std::vector< Group >
{
  /// Tries finding the given group by its id. Returns the group found, or
  /// 0 if there's no such group.
  Group * findGroup( unsigned id );
  Group const * findGroup( unsigned id ) const;
};

