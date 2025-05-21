/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "transliteration.hh"
#include "text.hh"
#include "folding.hh"

namespace Transliteration {


BaseTransliterationDictionary::BaseTransliterationDictionary( string const & id,
                                                              string const & name_,
                                                              QIcon icon_,
                                                              bool caseSensitive_ ):
  Dictionary::Class( id, vector< string >() ),
  name( name_ ),
  caseSensitive( caseSensitive_ )
{
  dictionaryIcon       = icon_;
  dictionaryIconLoaded = true;
}

string BaseTransliterationDictionary::getName() noexcept
{
  return name;
}

unsigned long BaseTransliterationDictionary::getArticleCount() noexcept
{
  return 0;
}

unsigned long BaseTransliterationDictionary::getWordCount() noexcept
{
  return 0;
}

sptr< Dictionary::WordSearchRequest > BaseTransliterationDictionary::prefixMatch( std::u32string const &,
                                                                                  unsigned long )
{
  return std::make_shared< Dictionary::WordSearchRequestInstant >();
}

sptr< Dictionary::DataRequest > BaseTransliterationDictionary::getArticle( std::u32string const &,
                                                                           vector< std::u32string > const &,
                                                                           std::u32string const &,
                                                                           bool )

{
  return std::make_shared< Dictionary::DataRequestInstant >( false );
}

sptr< Dictionary::WordSearchRequest >
BaseTransliterationDictionary::findHeadwordsForSynonym( std::u32string const & str )

{
  sptr< Dictionary::WordSearchRequestInstant > result = std::make_shared< Dictionary::WordSearchRequestInstant >();

  vector< std::u32string > alts = getAlternateWritings( str );

  for ( const auto & alt : alts ) {
    result->getMatches().push_back( alt );
  }

  return result;
}


void Table::ins( char const * from, char const * to )
{
  std::u32string fr = Text::toUtf32( std::string( from ) );

  insert( std::pair< std::u32string, std::u32string >( fr, Text::toUtf32( std::string( to ) ) ) );
}

void Table::ins( std::string const & from, std::string const & to )
{
  insert( std::pair< std::u32string, std::u32string >( Text::toUtf32( from ), Text::toUtf32( to ) ) );
}


TransliterationDictionary::TransliterationDictionary(
  string const & id, string const & name_, QIcon icon_, Table const & table_, bool caseSensitive_ ):
  BaseTransliterationDictionary( id, name_, icon_, caseSensitive_ ),
  table( table_ )
{
}

vector< std::u32string > TransliterationDictionary::getAlternateWritings( std::u32string const & str ) noexcept
{
  vector< std::u32string > results;

  std::u32string result, folded;
  std::u32string const * target;

  if ( caseSensitive ) {
    // Don't do any transform -- the transliteration is case-sensitive
    target = &str;
  }
  else {
    folded = Folding::applySimpleCaseOnly( str );
    target = &folded;
  }

  char32_t const * ptr = target->c_str();
  size_t left          = target->size();

  Table::const_iterator i;

  while ( left ) {
    unsigned x;

    for ( x = table.size(); x >= 1; --x ) {
      if ( left >= x ) {
        i = table.find( std::u32string( ptr, x ) );

        if ( i != table.end() ) {
          result.append( i->second );
          ptr += x;
          left -= x;
          break;
        }
      }
    }

    if ( !x ) {
      // No matches -- add this char as it is
      result.push_back( *ptr++ );
      --left;
    }
  }

  if ( result != *target ) {
    results.push_back( result );
  }

  return results;
}

} // namespace Transliteration
