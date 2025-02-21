//
// Created by xiaoyifang on 2025/2/21.
//

#ifndef GOLDENDICT_NG_FAVORITEMANAGER_HH
#define GOLDENDICT_NG_FAVORITEMANAGER_HH

#include <QString>
enum class FavoriteType {
  EMPTY,
  //current group is empty while other group is not empty
  EMPTY_OTHER,
  FULL,
  FULL_OTHER
};

class FavoriteManager
{

public:
  static FavoriteType determineFavoriteType( QString const & word, unsigned group );
  static FavoriteType determineFavoriteType( const QString & word, const QString & folder );
};


#endif //GOLDENDICT_NG_FAVORITEMANAGER_HH
