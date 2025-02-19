//
// Created by xiaoyifang on 2025/2/21.
//

#include "favoritemanager.hh"
#include "globalbroadcaster.hh"
FavoriteType FavoriteManager::determineFavoriteType( const QString & word, unsigned groupId )
{
  QString folder = GlobalBroadcaster::instance()->groupFolderMap[ groupId ];
  return determineFavoriteType( word, folder );
}

FavoriteType FavoriteManager::determineFavoriteType( const QString & word, const QString & folder )
{
  if ( !GlobalBroadcaster::instance()->wordFavoriteFolderMap.contains( word ) ) {
    return FavoriteType::EMPTY;
  }

  if ( GlobalBroadcaster::instance()->wordFavoriteFolderMap[ word ].contains( folder ) ) {
    if ( GlobalBroadcaster::instance()->wordFavoriteFolderMap[ word ].size() == 1 ) {
      return FavoriteType::FULL;
    }
    else {
      return FavoriteType::FULL_OTHER;
    }
  }
  return FavoriteType::EMPTY_OTHER;
}
