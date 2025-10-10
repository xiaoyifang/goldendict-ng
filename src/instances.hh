/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "config.hh"
#include "dict/dictionary.hh"
#include <QIcon>

// This complements Config, providing instances for the stored configurations.
// Simply put, it can convert groups to ones which hold references to
// dictionaries directly, instead of only having their ids, and can also convert
// them back.

namespace Instances {

using std::vector;

struct Group
{
  unsigned id;
  QString name, icon, favoritesFolder;
  QIcon iconData;
  QKeySequence shortcut;
  vector< sptr< Dictionary::Class > > dictionaries;

  /// Instantiates the given group from its configuration. If some dictionary
  /// wasn't found, it just skips it.
  Group( const Config::Group & cfgGroup,
         const std::vector< sptr< Dictionary::Class > > & allDictionaries,
         const Config::Group & inactiveGroup );

  /// Creates an empty group.
  explicit Group();

  Group( unsigned id, QString name_ );

  /// Makes the configuration group from the current contents.
  Config::Group makeConfigGroup();

  /// Makes an icon object for this group, based on the icon's name or iconData.
  QIcon makeIcon() const;

  /// Remove id's if not presented in group dictionaries
  void checkMutedDictionaries( Config::DictionarySets * mutedDictionaries ) const;
};

struct Groups: vector< Group >
{
  /// Tries finding the given group by its id.
  /// @return The group found, or nullptr if there's no such group.
  const Group * findGroup( unsigned id ) const;
};

/// Adds any dictionaries not already present in the given group or in
/// inactiveDictionaires to its end. Meant to be used with dictionaryOrder
/// special group.
void complementDictionaryOrder( Group & dictionaryOrder,
                                const Group & inactiveDictionaries,
                                const vector< sptr< Dictionary::Class > > & allDictionaries );

/// For any dictionaries present in the group, updates their names to match
/// the dictionaries they refer to in their current form, if they exist.
/// If the dictionary instance can't be located, the name is left untouched.
void updateNames( Config::Group &, const vector< sptr< Dictionary::Class > > & allDictionaries );

/// Does updateNames() for a set of given groups.
void updateNames( Config::Groups &, const vector< sptr< Dictionary::Class > > & allDictionaries );

/// Does updateNames() for any relevant dictionary groups present in the
/// configuration.
void updateNames( Config::Class &, const vector< sptr< Dictionary::Class > > & allDictionaries );

/// Creates icon from icon data. Used by Group, but also by others who work
/// with icon data directly.
QIcon iconFromData( const QByteArray & );

} // namespace Instances
