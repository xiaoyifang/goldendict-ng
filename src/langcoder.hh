#pragma once

#include <QIcon>
#include "text.hh"

struct GDLangCode
{
  QString code2;     // ISO 639-1 -> always 2 letters thus code2
  std::string code3; // ISO 639-2B ( http://www.loc.gov/standards/iso639-2/ )
  int isRTL;         // Right-to-left writing; 0 - no, 1 - yes, -1 - let Qt define
  std::string lang;  // Language name in English
};

class LangCoder
{
public:

  static quint32 code2toInt( const char code[ 2 ] )
  {
    return ( ( (quint32)code[ 1 ] ) << 8 ) + (quint32)code[ 0 ];
  }

  static quint32 code2toInt( QString code )
  {
    if ( code.size() < 2 )
      return 0;
    auto c = code.toLatin1();

    return ( ( (quint32)c[ 1 ] ) << 8 ) + (quint32)c[ 0 ];
  }

  static QString intToCode2( quint32 );

  /// Finds the id for the given language name, written in english. The search
  /// is case- and punctuation insensitive.
  static quint32 findIdForLanguage( const std::u32string & );

  static quint32 findIdForLanguageCode3( const std::string & );

  /// find id pairs like en-zh in dictioanry name
  static std::pair< quint32, quint32 > findLangIdPairFromName( const QString & );
  static std::pair< quint32, quint32 > findLangIdPairFromPath( const std::string & );

  static quint32 guessId( const QString & lang );

  /// Returns decoded name of language or empty string if not found.
  static QString decode( quint32 _code );

  /// Return true for RTL languages
  static bool isLanguageRTL( quint32 code );

private:
  static QMap< QString, GDLangCode > LANG_CODE_MAP;
  static bool code2Exists( const QString & _code );
};

///////////////////////////////////////////////////////////////////////////////

#define LangCodeRole Qt::UserRole
