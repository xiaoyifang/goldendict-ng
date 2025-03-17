/* This file is (c) 2021–2025 The GoldenDict-ng Community
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QMutex>
#include <list>
#include "sptr.hh"

template <typename T>
class concurrent_list
{
    QMutex mutex;
    std::list<T> list;

public:
    void push_back(const T& v)
    {
        QMutexLocker locker(&mutex);
        list.push_back(v);
    }

    void remove(const T& v)
    {
        QMutexLocker locker(&mutex);
        list.remove(v);
    }

    void clear()
    {
        QMutexLocker locker(&mutex);
        list.clear();
    }
};