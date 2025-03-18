/* This file is (c) 2021–2025 The GoldenDict-ng Community
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once
#include <QMutex>
#include <QMutexLocker>
#include <list>
using std::list;

template< typename T >
class concurrent_list
{
  QMutex mutex{};
  std::list< T > list;

public:

  void push_back( const T & v )
  {
    QMutexLocker locker( &mutex );
    list.push_back( v );
  }

  void remove( const T & v )
  {
    QMutexLocker locker( &mutex );
    list.remove( v );
  }

  void clear()
  {
    QMutexLocker locker( &mutex );
    list.clear();
  }

  bool empty()
  {
    QMutexLocker locker( &mutex );
    return list.empty();
  }


  //use snapshot to avoid locking
  //and then iterate over the snapshot which will not be affected by other threads
  std::list< T > snapshot()
  {
    QMutexLocker locker( &mutex );
    return list;
  }
};
