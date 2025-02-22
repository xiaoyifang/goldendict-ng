/* This file is (c) 2025 gd-ng community
* Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QString>

class FavoriteManager
{

public:
  static bool isHeadwordPresent( const QString & word, const QString & folder );
  static bool isHeadwordPresent( const QString & word, unsigned groupId );
};
