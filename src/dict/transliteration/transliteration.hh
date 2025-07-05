/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dictionary.hh"
#include <map>

namespace Transliteration {

using std::map;
using std::string;
using std::vector;


/// This is a base dictionary class for simple transliteratons
class BaseTransliterationDictionary: public Dictionary::Class
{
  string name;

protected:
  bool caseSensitive;

public:

  BaseTransliterationDictionary( const string & id, const string & name, QIcon icon, bool caseSensitive = true );

  virtual string getName() noexcept;

  virtual unsigned long getArticleCount() noexcept;

  virtual unsigned long getWordCount() noexcept;

  virtual vector< std::u32string > getAlternateWritings( const std::u32string & ) noexcept = 0;

  virtual sptr< Dictionary::WordSearchRequest > findHeadwordsForSynonym( const std::u32string & );

  virtual sptr< Dictionary::WordSearchRequest > prefixMatch( const std::u32string &, unsigned long );

  virtual sptr< Dictionary::DataRequest >
  getArticle( const std::u32string &, const vector< std::u32string > &, const std::u32string &, bool );
};


class Table: public map< std::u32string, std::u32string >
{

protected:

  /// Inserts new entry into index. from and to are UTF8-encoded strings.
  void ins( const char * from, const char * to );

  /// Inserts new entry into index. from and to are UTF32-encoded strings.
  void ins( const std::u32string & from, const std::u32string & to )
  {
    this->insert( { from, to } );
  }

  /// Inserts new entry into index. from and to are std::string (UTF8-encoded).
  void ins( const std::string & from, const std::string & to );
};


/// A base dictionary class for table based transliteratons
class TransliterationDictionary: public BaseTransliterationDictionary
{
  const Table & table;

public:

  TransliterationDictionary(
    const string & id, const string & name, QIcon icon, const Table & table, bool caseSensitive = true );

  virtual vector< std::u32string > getAlternateWritings( const std::u32string & ) noexcept;
};

} // namespace Transliteration
