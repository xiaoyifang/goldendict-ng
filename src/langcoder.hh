#ifndef LANGCODER_H
#define LANGCODER_H

#include <QtGui>
#include "wstring.hh"

struct GDLangCode
{
  QString code;  // ISO 639-1
  QString code3; // ISO 639-2B ( http://www.loc.gov/standards/iso639-2/ )
  int isRTL;     // Right-to-left writing; 0 - no, 1 - yes, -1 - let Qt define
  QString lang;  // Language name in English
};


template< typename T, int N >
inline int arraySize( T ( & )[ N ] )
{
  return N;
}

struct LangStruct
{
  int order;
  quint32 code;
  QIcon icon;
  QString lang;
};

class LangCoder
{
public:
  LangCoder();

  static quint32 code2toInt(const char code[2])
  { return ( ((quint32)code[1]) << 8 ) + (quint32)code[0]; }

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
  static quint32 findIdForLanguage( gd::wstring const & );

  static quint32 findIdForLanguageCode3( std::string );

  static QPair<quint32,quint32> findIdsForName( QString const & );
  static QPair<quint32,quint32> findIdsForFilename( QString const & );

  static quint32 guessId( const QString & lang );

  /// Returns decoded name of language or empty string if not found.
  static QString decode(quint32 code);
  /// Returns icon for language or empty string if not found.
  static QIcon icon(quint32 code);

  /// Return true for RTL languages
  static bool isLanguageRTL( quint32 code );

private:
  QMap< QString, GDLangCode > codeMap;
};

///////////////////////////////////////////////////////////////////////////////

#define LangCodeRole Qt::UserRole


#endif // LANGCODER_H
