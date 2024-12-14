//
// Created by xiaoyifang on 2024/12/10.
//

#ifndef GOLDENDICT_NG_XAPIANIDX_H
#define GOLDENDICT_NG_XAPIANIDX_H
#include "xapian.h"
#include <map>
#include <string>

class XapianIdx
{

public:
  void buildIdx( const std::string & path, std::map< std::string, std::string > & indexedWords );
};


#endif //GOLDENDICT_NG_XAPIANIDX_H
