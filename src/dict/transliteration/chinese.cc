/* This file is (c) 2015 Zhe Wang <0x1997@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "chinese.hh"
#include <algorithm>
#include <stdexcept>
#include <QCoreApplication>
#include <opencc/opencc.h>
#include "folding.hh"
#include "transliteration.hh"
#include "text.hh"

namespace ChineseTranslit {

/// Converts the input by applying a chain of OpenCC configurations in sequence.
/// Chaining allows "transparent" conversion between arbitrary Chinese variants
/// (e.g. simplified -> traditional -> Japanese Shinjitai), so the caller does
/// not need to know beforehand which variant the input is written in.
class CharacterConversionDictionary: public Transliteration::BaseTransliterationDictionary
{
  std::vector< opencc_t > converters;

public:

  CharacterConversionDictionary( const std::string & id,
                                 const std::string & name,
                                 QIcon icon,
                                 const std::vector< QString > & openccConfigs );
  ~CharacterConversionDictionary();

  std::vector< std::u32string > getAlternateWritings( const std::u32string & ) noexcept override;
};

CharacterConversionDictionary::CharacterConversionDictionary( const std::string & id,
                                                              const std::string & name_,
                                                              QIcon icon_,
                                                              const std::vector< QString > & openccConfigs ):
  Transliteration::BaseTransliterationDictionary( id, name_, icon_, false )
{
  for ( size_t i = 0; i < openccConfigs.size(); ++i ) {
    const QString & openccConfig = openccConfigs[ i ];
    // The last config is the primary conversion step; the ones before it are
    // optional normalization steps (e.g. jp2t before t2s).
    const bool isPrimary = ( i + 1 == openccConfigs.size() );

    opencc_t converter = NULL;
    try {
      converter = opencc_open( openccConfig.toLocal8Bit().constData() );
      if ( converter == reinterpret_cast< opencc_t >( -1 ) ) {
        qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
                  openccConfig.toLocal8Bit().constData(),
                  opencc_error() );
        converter = NULL;
      }
    }
    catch ( std::exception & e ) {
      qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
                openccConfig.toLocal8Bit().constData(),
                e.what() );
      converter = NULL;
    }
    catch ( ... ) {
      qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s",
                openccConfig.toLocal8Bit().constData() );
      converter = NULL;
    }

    if ( converter == NULL ) {
      if ( isPrimary ) {
        // Without the primary step the dictionary would return wrong results
        // (or none), so disable it entirely. Other variants are unaffected
        // since each dictionary owns its own converters.
        qWarning( "CharacterConversionDictionary: disabling conversion, config %s is unavailable",
                  openccConfig.toLocal8Bit().constData() );
        for ( opencc_t opened : converters ) {
          opencc_close( opened );
        }
        converters.clear();
        return;
      }

      // An optional normalization step (e.g. the Japanese jp2t data) is
      // missing; skip it and keep the rest of the chain working so the
      // original simplified/traditional conversion is not broken.
      qWarning( "CharacterConversionDictionary: skipping unavailable config %s",
                openccConfig.toLocal8Bit().constData() );
      continue;
    }

    converters.push_back( converter );
  }
}

CharacterConversionDictionary::~CharacterConversionDictionary()
{
  for ( opencc_t converter : converters ) {
    if ( converter != NULL && converter != reinterpret_cast< opencc_t >( -1 ) ) {
      opencc_close( converter );
    }
  }
}

std::vector< std::u32string > CharacterConversionDictionary::getAlternateWritings( const std::u32string & str ) noexcept
{
  std::vector< std::u32string > results;

  if ( converters.empty() ) {
    return results;
  }

  std::u32string folded = Folding::applySimpleCaseOnly( str );
  std::string output    = Text::toUtf8( folded );

  try {
    for ( opencc_t converter : converters ) {
      char * tmp = opencc_convert_utf8( converter, output.c_str(), output.length() );
      if ( tmp == nullptr ) {
        // Conversion failed (e.g. malformed UTF-8 input). Keep the previous
        // output so the result is never empty or truncated mid-chain.
        qWarning( "OpenCC: conversion failed %s", opencc_error() );
        continue;
      }
      output.assign( tmp );
      opencc_convert_utf8_free( tmp );
    }
  }
  catch ( std::exception & ex ) {
    // This method is noexcept, so an escaping exception would terminate the
    // whole application. Swallow it and fall back to the last good output.
    qWarning( "OpenCC: conversion failed %s", ex.what() );
  }
  catch ( ... ) {
    qWarning( "OpenCC: conversion failed with an unknown error" );
  }

  std::u32string result = Text::toUtf32( output );

  // Skip empty results and results identical to the input, so a word already
  // written in the target variant (or one that did not change) does not
  // produce an empty or duplicate entry.
  if ( !result.empty() && result != folded && std::find( results.begin(), results.end(), result ) == results.end() ) {
    results.push_back( result );
  }

  return results;
}

std::vector< sptr< Dictionary::Class > > makeDictionaries( const Config::Chinese & cfg )

{
  std::vector< sptr< Dictionary::Class > > result;

#ifdef Q_OS_LINUX
  QString configDir = "";
#else
  QString configDir = Config::getOpenCCDir();
  if ( !configDir.isEmpty() ) {
    configDir += "/";
  }
#endif

  if ( cfg.enable ) {
    if ( cfg.enableSCToTWConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "bf1c33a59cbacea8f39b5b5475787cfd",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Taiwan variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/tc.svg" ),
        std::vector< QString >{ configDir + "s2tw.json" } ) );
    }

    if ( cfg.enableSCToHKConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "9e0681fb9e1c0b6c90e6fb46111d96b5",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Hong Kong variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/hk.svg" ),
        std::vector< QString >{ configDir + "s2hk.json" } ) );
    }

    if ( cfg.enableTCToSCConversion ) {
      // Japanese Shinjitai input is first normalized to traditional Chinese (jp2t)
      // before being converted to simplified, so that Japanese Kanji can also
      // match simplified Chinese entries.
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "0db536ce0bdc52ea30d11a82c5db4a27",
        QCoreApplication::translate( "ChineseConversion", "Traditional to simplified Chinese conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/sc.svg" ),
        std::vector< QString >{ configDir + "jp2t.json", configDir + "t2s.json" } ) );
    }

    if ( cfg.enableJapaneseConversion ) {
      // Simplified input is first converted to traditional (s2t) because t2jp
      // does not recognize simplified characters, then mapped to Japanese
      // Shinjitai (t2jp). This makes simplified and traditional input both work.
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "7d8e9f1a2b3c4d5e6f0a1b2c3d4e5f60",
        QCoreApplication::translate( "ChineseConversion", "Chinese to Japanese Shinjitai conversion" ).toUtf8().data(),
        QIcon( ":/flags/jp.png" ),
        std::vector< QString >{ configDir + "s2t.json", configDir + "t2jp.json" } ) );
    }
  }

  return result;
}

} // namespace ChineseTranslit
