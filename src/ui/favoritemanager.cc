/* This file is (c) 2025 gd-ng community
* Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "favoritemanager.hh"
#include "globalbroadcaster.hh"


bool FavoriteManager::isHeadwordPresent( const QString & word, const QString & folder )
{
  if ( !GlobalBroadcaster::instance()->wordFavoriteFolderMap.contains( word ) ) {
    return false;
  }
  if ( GlobalBroadcaster::instance()->wordFavoriteFolderMap[ word ].contains( folder ) ) {
    return true;
  }
  return false;
}

bool FavoriteManager::isHeadwordPresent( const QString & word, unsigned groupId )
{
  if ( word.isEmpty() ) {
    return false;
  }
  QString folder = GlobalBroadcaster::instance()->groupFolderMap[ groupId ];
  return isHeadwordPresent( word, folder );
}
