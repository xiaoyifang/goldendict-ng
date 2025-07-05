/* This file is (c) 2015 Zhe Wang <0x1997@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "chinese.hh"
#include <stdexcept>
#include <QCoreApplication>
#include <opencc/opencc.h>
#include "folding.hh"
#include "transliteration.hh"
#include "text.hh"

namespace ChineseTranslit {

class CharacterConversionDictionary: public Transliteration::BaseTransliterationDictionary
{
  // #ifdef Q_OS_MAC
  opencc_t converter;
  // #else
  //   opencc::SimpleConverter* converter;
  // #endif

public:

  CharacterConversionDictionary( const std::string & id,
                                 const std::string & name,
                                 QIcon icon,
                                 const QString & openccConfig );
  ~CharacterConversionDictionary();

  std::vector< std::u32string > getAlternateWritings( const std::u32string & ) noexcept override;
};

CharacterConversionDictionary::CharacterConversionDictionary( const std::string & id,
                                                              const std::string & name_,
                                                              QIcon icon_,
                                                              const QString & openccConfig ):
  Transliteration::BaseTransliterationDictionary( id, name_, icon_, false ),
  converter( NULL )
{
  try {
    // #ifdef Q_OS_MAC
    converter = opencc_open( openccConfig.toLocal8Bit().constData() );
    if ( converter == reinterpret_cast< opencc_t >( -1 ) ) {
      qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
                openccConfig.toLocal8Bit().constData(),
                opencc_error() );
    }
    // #else
    //     converter = new opencc::SimpleConverter( openccConfig.toLocal8Bit().constData() );
    // #endif
  }
  catch ( std::runtime_error & e ) {
    qWarning( "CharacterConversionDictionary: failed to initialize OpenCC from config %s: %s",
              openccConfig.toLocal8Bit().constData(),
              e.what() );
  }
}

CharacterConversionDictionary::~CharacterConversionDictionary()
{
  // #ifdef Q_OS_MAC
  if ( converter != NULL && converter != reinterpret_cast< opencc_t >( -1 ) ) {
    opencc_close( converter );
  }
  // #else
  //   if ( converter != NULL )
  //     delete converter;
  // #endif
}

std::vector< std::u32string > CharacterConversionDictionary::getAlternateWritings( const std::u32string & str ) noexcept
{
  std::vector< std::u32string > results;

  if ( converter != NULL ) {
    std::u32string folded = Folding::applySimpleCaseOnly( str );
    std::string input     = Text::toUtf8( folded );
    std::string output;
    std::u32string result;

    try {
      // #ifdef Q_OS_MAC
      if ( converter != NULL && converter != reinterpret_cast< opencc_t >( -1 ) ) {
        char * tmp = opencc_convert_utf8( converter, input.c_str(), input.length() );
        if ( tmp ) {
          output = std::string( tmp );
          opencc_convert_utf8_free( tmp );
        }
        else {
          qWarning( "OpenCC: conversion failed %s", opencc_error() );
        }
      }
      // #else
      //       output = converter->Convert( input );
      // #endif
      result = Text::toUtf32( output );
    }
    catch ( std::exception & ex ) {
      qWarning( "OpenCC: conversion failed %s", ex.what() );
    }

    if ( !result.empty() && result != folded ) {
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
    if ( cfg.enableSCToTWConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "bf1c33a59cbacea8f39b5b5475787cfd",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Taiwan variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/tc.svg" ),
        configDir + "s2tw.json" ) );
    }

    if ( cfg.enableSCToHKConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "9e0681fb9e1c0b6c90e6fb46111d96b5",
        QCoreApplication::translate( "ChineseConversion",
                                     "Simplified to traditional Chinese (Hong Kong variant) conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/hk.svg" ),
        configDir + "s2hk.json" ) );
    }

    if ( cfg.enableTCToSCConversion ) {
      result.push_back( std::make_shared< CharacterConversionDictionary >(
        "0db536ce0bdc52ea30d11a82c5db4a27",
        QCoreApplication::translate( "ChineseConversion", "Traditional to simplified Chinese conversion" )
          .toUtf8()
          .data(),
        QIcon( ":/icons/sc.svg" ),
        configDir + "t2s.json" ) );
    }
  }

  return result;
}

} // namespace ChineseTranslit
