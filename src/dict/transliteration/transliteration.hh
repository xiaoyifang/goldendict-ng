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

  BaseTransliterationDictionary( string const & id, string const & name, QIcon icon, bool caseSensitive = true );

  virtual string getName() noexcept;

  virtual unsigned long getArticleCount() noexcept;

  virtual unsigned long getWordCount() noexcept;

  virtual vector< std::u32string > getAlternateWritings( std::u32string const & ) noexcept = 0;

  virtual sptr< Dictionary::WordSearchRequest > findHeadwordsForSynonym( std::u32string const & );

  virtual sptr< Dictionary::WordSearchRequest > prefixMatch( std::u32string const &, unsigned long );

  virtual sptr< Dictionary::DataRequest >
  getArticle( std::u32string const &, vector< std::u32string > const &, std::u32string const &, bool );
};


class Table: public map< std::u32string, std::u32string >
{

protected:

  /// Inserts new entry into index. from and to are UTF8-encoded strings.
  void ins( char const * from, char const * to );

  /// Inserts new entry into index. from and to are UTF32-encoded strings.
  void ins( std::u32string const & from, std::u32string const & to )
  {
    this->insert( { from, to } );
  }
};


/// A base dictionary class for table based transliteratons
class TransliterationDictionary: public BaseTransliterationDictionary
{
  Table const & table;

public:

  TransliterationDictionary(
    string const & id, string const & name, QIcon icon, Table const & table, bool caseSensitive = true );

  virtual vector< std::u32string > getAlternateWritings( std::u32string const & ) noexcept;
};

} // namespace Transliteration
