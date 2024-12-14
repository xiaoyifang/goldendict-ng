//
// Created by xiaoyifang on 2024/12/14.
//

#ifndef GOLDENDICT_NG_XAPIANIDXREADER_HH
#define GOLDENDICT_NG_XAPIANIDXREADER_HH
#include "xapian.h"
#include <string>

class XapianIdxReader
{
  XapianIdxReader( std::string path );

private:
  Xapian::Database db;
};


#endif //GOLDENDICT_NG_XAPIANIDXREADER_HH
