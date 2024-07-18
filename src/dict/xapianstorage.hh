/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "xapian.h"


#include <string>


namespace XapianStorage {


/// This class writes data blocks in chunks.
class Writer
{
  QString _filename;
  Xapian::WritableDatabase _db;
  bool _isClosed = false;

public:
  explicit Writer( QString & );

  void addDocument(uint32_t , std::string const &,std::string const prefix=""  );

  void commit();


private:

};

/// This class reads data blocks previously written by Writer.
class Reader
{
  QString _filename;
  Xapian::Database _db;
public:
  Reader( QString & );

  QList< uint32_t > getDocument( std::string const &);
};

} // namespace XapianStorage
