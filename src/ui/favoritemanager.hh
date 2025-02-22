/* This file is (c) 2025 gd-ng community
* Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

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
  static bool isHeadwordPresent( const QString & word, const QString & folder );
  static bool isHeadwordPresent( const QString & word, unsigned group );
};
