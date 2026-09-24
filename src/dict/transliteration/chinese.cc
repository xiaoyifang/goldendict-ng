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

/// Converts the input by applying one or more OpenCC configuration chains.
///
/// Each chain is applied in sequence, which allows "transparent" conversion
/// between arbitrary Chinese variants (e.g. simplified -> traditional ->
/// Japanese Shinjitai), so the caller does not need to know beforehand which
/// variant the input is written in. Several chains are evaluated in parallel
/// and their outputs are merged, so a normalization step that would override
/// the plain conversion (e.g. jp2t mapping 連 to 聯, hiding the correct t2s
/// result 连) can never suppress the plain result: it is merely added.
class CharacterConversionDictionary: public Transliteration::BaseTransliterationDictionary
{
  // Each inner vector holds the converters of one independent chain.
  std::vector< std::vector< opencc_t > > chains;

public:

  CharacterConversionDictionary( const std::string & id,
                                 const std::string & name,
                                 QIcon icon,
                                 const std::vector< std::vector< QString > > & openccConfigChains );
  ~CharacterConversionDictionary();

  std::vector< std::u32string > getAlternateWritings( const std::u32string & ) noexcept override;
};

namespace {

/// Opens a single OpenCC configuration, returning nullptr on failure. OpenCC
/// reports a failure by returning a (opencc_t)-1 handle instead of throwing,
/// and may also throw on a malformed configuration file.
opencc_t openConverter( const QString & openccConfig )
{
  // opencc_open() reports a failure by returning this sentinel handle instead
  // of throwing (see the opencc.h contract), so it must be checked explicitly.
  // There is no safer way to express it: opencc_t is an opaque void*.
  const opencc_t invalidConverter = reinterpret_cast< opencc_t >( -1 );

  try {
    opencc_t converter = opencc_open( openccConfig.toLocal8Bit().constData() );
    if ( converter != invalidConverter ) {
      return converter;
    }
    qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
              openccConfig.toLocal8Bit().constData(),
              opencc_error() );
  }
  catch ( std::exception & e ) {
    qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
              openccConfig.toLocal8Bit().constData(),
              e.what() );
  }
  catch ( ... ) {
    qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s",
              openccConfig.toLocal8Bit().constData() );
  }

  return nullptr;
}

/// Opens one conversion chain. The last config is the primary conversion step
/// while the ones before it are optional normalization steps (e.g. jp2t before
/// t2s): an unavailable optional step is skipped, but an unavailable primary
/// step makes the whole chain unusable and an empty vector is returned.
std::vector< opencc_t > openChain( const std::vector< QString > & chainConfigs )
{
  std::vector< opencc_t > chain;

  for ( size_t i = 0; i < chainConfigs.size(); ++i ) {
    const QString & openccConfig = chainConfigs[ i ];
    opencc_t converter           = openConverter( openccConfig );

    if ( converter != nullptr ) {
      chain.push_back( converter );
      continue;
    }

    // An optional normalization step (e.g. the Japanese jp2t data) is missing;
    // skip it and keep the rest of the chain working so the original
    // simplified/traditional conversion is not broken.
    if ( i + 1 != chainConfigs.size() ) {
      qWarning( "CharacterConversionDictionary: skipping unavailable config %s",
                openccConfig.toLocal8Bit().constData() );
      continue;
    }

    // Without the primary step this chain would return wrong results (or none),
    // so drop the whole chain. The other chains of this dictionary keep working,
    // so the plain conversion stays intact.
    qWarning( "CharacterConversionDictionary: dropping conversion chain, config %s is unavailable",
              openccConfig.toLocal8Bit().constData() );
    for ( opencc_t opened : chain ) {
      opencc_close( opened );
    }
    chain.clear();
    break;
  }

  return chain;
}

} // namespace

CharacterConversionDictionary::CharacterConversionDictionary(
  const std::string & id,
  const std::string & name_,
  QIcon icon_,
  const std::vector< std::vector< QString > > & openccConfigChains ):
  Transliteration::BaseTransliterationDictionary( id, name_, icon_, false )
{
  for ( const std::vector< QString > & chainConfigs : openccConfigChains ) {
    std::vector< opencc_t > chain = openChain( chainConfigs );
    if ( !chain.empty() ) {
      chains.push_back( std::move( chain ) );
    }
  }
}

CharacterConversionDictionary::~CharacterConversionDictionary()
{
  // Only successfully opened converters are ever stored, so no validity check
  // is needed here.
  for ( const std::vector< opencc_t > & chain : chains ) {
    for ( opencc_t converter : chain ) {
      opencc_close( converter );
    }
  }
}

std::vector< std::u32string > CharacterConversionDictionary::getAlternateWritings( const std::u32string & str ) noexcept
{
  std::vector< std::u32string > results;

  if ( chains.empty() ) {
    return results;
  }

  std::u32string folded = Folding::applySimpleCaseOnly( str );

  for ( const std::vector< opencc_t > & chain : chains ) {
    std::string output = Text::toUtf8( folded );

    try {
      for ( opencc_t converter : chain ) {
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
    // produce an empty or duplicate entry. Results are also deduplicated
    // across chains.
    if ( !result.empty() && result != folded && std::find( results.begin(), results.end(), result ) == results.end() ) {
      results.push_back( result );
    }
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
    // Every dictionary converts to one target variant and accepts any of the
    // other variants as input. Two chains are registered per Chinese variant:
    // the plain one, plus one that normalizes Japanese Shinjitai input to
    // traditional Chinese (jp2t) first, so Japanese Kanji can also match
    // simplified, Taiwan and Hong Kong entries. The plain chain comes first and
    // both chains are kept, so Japanese normalization can only add a candidate
    // and can never override the plain conversion result.
    if ( cfg.enableSCToTWConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "bf1c33a59cbacea8f39b5b5475787cfd",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Taiwan variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/tc.svg" ),
        std::vector< std::vector< QString > >{ { configDir + "s2tw.json" },
                                               { configDir + "jp2t.json", configDir + "s2tw.json" } } ) );
    }

    if ( cfg.enableSCToHKConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "9e0681fb9e1c0b6c90e6fb46111d96b5",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Hong Kong variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/hk.svg" ),
        std::vector< std::vector< QString > >{ { configDir + "s2hk.json" },
                                               { configDir + "jp2t.json", configDir + "s2hk.json" } } ) );
    }

    if ( cfg.enableTCToSCConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "0db536ce0bdc52ea30d11a82c5db4a27",
        QCoreApplication::translate( "ChineseConversion", "Traditional to simplified Chinese conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/sc.svg" ),
        std::vector< std::vector< QString > >{ { configDir + "t2s.json" },
                                               { configDir + "jp2t.json", configDir + "t2s.json" } } ) );
    }

    if ( cfg.enableJapaneseConversion ) {
      // Simplified input is first converted to traditional (s2t) because t2jp
      // does not recognize simplified characters, then mapped to Japanese
      // Shinjitai (t2jp). This makes simplified and traditional input both work.
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "7d8e9f1a2b3c4d5e6f0a1b2c3d4e5f60",
        QCoreApplication::translate( "ChineseConversion", "Chinese to Japanese Shinjitai conversion" ).toUtf8().data(),
        QIcon( ":/icons/jpc.svg" ),
        std::vector< std::vector< QString > >{ { configDir + "s2t.json", configDir + "t2jp.json" } } ) );
    }
  }

  return result;
}

} // namespace ChineseTranslit
