/* This file is (c) 2021–2025 The GoldenDict-ng Community
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once
#include <QMutex>
#include <list>
#include <map>
#include "sptr.hh"
using std::list;

template< typename T >
class concurrent_list
{
  QMutex mutex;
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

  bool empty() const
  {
    QMutexLocker lock( &mutex );
    return list.empty();
  }
  typename std::list< T >::iterator begin()
  {
    QMutexLocker locker( &mutex );
    return list.begin();
  }

  typename std::list< T >::iterator end()
  {
    QMutexLocker locker( &mutex );
    return list.end();
  }

  typename std::list< T >::const_iterator begin() const
  {
    QMutexLocker locker( &mutex );
    return list.cbegin();
  }

  typename std::list< T >::const_iterator end() const
  {
    QMutexLocker locker( &mutex );
    return list.cend();
  }

  typename std::list< T >::const_iterator cbegin() const
  {
    QMutexLocker locker( &mutex );
    return list.cbegin();
  }

  typename std::list< T >::const_iterator cend() const
  {
    QMutexLocker locker( &mutex );
    return list.cend();
  }

  std::list< T > snapshot() const
  {
    QMutexLocker locker( &mutex );
    return list;
  }
};
