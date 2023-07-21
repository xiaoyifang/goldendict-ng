/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "instances.hh"
#include <set>
#include <QBuffer>

namespace Instances {

using std::set;
using std::string;

Group::Group( Config::Group const & cfgGroup,
              std::vector< sptr< Dictionary::Class > > const & allDictionaries,
              Config::Group const & inactiveGroup ):
  id( cfgGroup.id ),
  name( cfgGroup.name ),
  icon( cfgGroup.icon ),
  favoritesFolder( cfgGroup.favoritesFolder ),
  shortcut( cfgGroup.shortcut )
{
  if ( !cfgGroup.iconData.isEmpty() )
    iconData = iconFromData( cfgGroup.iconData );

  QMap< string, sptr< Dictionary::Class > > groupDicts;
  QVector< string > dictOrderList;
  auto dictMap = Dictionary::dictToMap( allDictionaries );

  for ( auto const & dict : cfgGroup.dictionaries ) {
    std::string dictId = dict.id.toStdString();

    if ( dictMap.contains( dictId ) ) {
      groupDicts.insert( dictId, dictMap[ dictId ] );
      dictOrderList.push_back( dictId );
    }
  }

  // Remove inactive dictionaries
  if ( !inactiveGroup.dictionaries.isEmpty() ) {
    set< string, std::less<> > inactiveSet;
    for ( auto const & dict : inactiveGroup.dictionaries ) {
      string dictId = dict.id.toStdString();
      groupDicts.remove( dictId );
      dictOrderList.removeOne( dictId );
    }
  }
  for ( const auto & dictId : dictOrderList ) {
    dictionaries.push_back( groupDicts[ dictId ] );
  }
}

Group::Group( QString const & name_ ):
  id( 0 ),
  name( name_ )
{
}

Group::Group( unsigned id_, QString const & name_ ):
  id( id_ ),
  name( name_ )
{
}

Config::Group Group::makeConfigGroup()
{
  Config::Group result;

  result.id              = id;
  result.name            = name;
  result.icon            = icon;
  result.shortcut        = shortcut;
  result.favoritesFolder = favoritesFolder;

  if ( !iconData.isNull() ) {
    QDataStream stream( &result.iconData, QIODevice::WriteOnly );

    stream << iconData;
  }

  for ( auto const & dict : dictionaries ) {
    result.dictionaries.push_back(
      Config::DictionaryRef( dict->getId().c_str(), QString::fromUtf8( dict->getName().c_str() ) ) );
  }

  return result;
}

QIcon Group::makeIcon() const
{
  if ( !iconData.isNull() )
    return iconData;

  QIcon i = icon.size() ? QIcon( ":/flags/" + icon ) : QIcon();

  return i;
}

void Group::checkMutedDictionaries( Config::MutedDictionaries * mutedDictionaries ) const
{
  Config::MutedDictionaries temp;

  for ( auto const & dict : dictionaries ) {
    QString id = QString::fromStdString( dict->getId() );
    if ( mutedDictionaries->contains( id ) )
      temp.insert( id );
  }
  *mutedDictionaries = temp;
}

Group * Groups::findGroup( unsigned id )
{
  for ( unsigned x = 0; x < size(); ++x )
    if ( operator[]( x ).id == id )
      return &( operator[]( x ) );

  return nullptr;
}

Group const * Groups::findGroup( unsigned id ) const
{
  for ( unsigned x = 0; x < size(); ++x )
    if ( operator[]( x ).id == id )
      return &( operator[]( x ) );

  return nullptr;
}

void complementDictionaryOrder( Group & group,
                                Group const & inactiveDictionaries,
                                vector< sptr< Dictionary::Class > > const & dicts )
{
  set< string, std::less<> > presentIds;

  for ( unsigned x = group.dictionaries.size(); x--; )
    presentIds.insert( group.dictionaries[ x ]->getId() );

  for ( unsigned x = inactiveDictionaries.dictionaries.size(); x--; )
    presentIds.insert( inactiveDictionaries.dictionaries[ x ]->getId() );

  for ( unsigned x = 0; x < dicts.size(); ++x ) {
    if ( presentIds.find( dicts[ x ]->getId() ) == presentIds.end() )
      group.dictionaries.push_back( dicts[ x ] );
  }
}

void updateNames( Config::Group & group, vector< sptr< Dictionary::Class > > const & allDictionaries )
{

  for ( unsigned x = group.dictionaries.size(); x--; ) {
    std::string id = group.dictionaries[ x ].id.toStdString();

    for ( unsigned y = allDictionaries.size(); y--; )
      if ( allDictionaries[ y ]->getId() == id ) {
        group.dictionaries[ x ].name = QString::fromUtf8( allDictionaries[ y ]->getName().c_str() );
        break;
      }
  }
}

void updateNames( Config::Groups & groups, vector< sptr< Dictionary::Class > > const & allDictionaries )
{
  for ( int x = 0; x < groups.size(); ++x )
    updateNames( groups[ x ], allDictionaries );
}

void updateNames( Config::Class & cfg, vector< sptr< Dictionary::Class > > const & allDictionaries )
{
  updateNames( cfg.dictionaryOrder, allDictionaries );
  updateNames( cfg.inactiveDictionaries, allDictionaries );
  updateNames( cfg.groups, allDictionaries );
}

QIcon iconFromData( QByteArray const & iconData )
{
  QDataStream stream( iconData );

  QIcon result;

  stream >> result;

  return result;
}

} // namespace Instances
