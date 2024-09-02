/* This file is (c) 2008-2013 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "langcoder.hh"
#include "language.hh"
#include "utf8.hh"

#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>

#ifdef _MSC_VER
  #include <stub_msvc.h>
#endif
// Language codes

QMap< QString, GDLangCode > LangCoder::LANG_CODE_MAP = {
  { "aa", { "aa", "aar", -1, "Afar" } },
  { "ab", { "ab", "abk", -1, "Abkhazian" } },
  { "ae", { "ae", "ave", -1, "Avestan" } },
  { "af", { "af", "afr", -1, "Afrikaans" } },
  { "ak", { "ak", "aka", -1, "Akan" } },
  { "am", { "am", "amh", -1, "Amharic" } },
  { "an", { "an", "arg", -1, "Aragonese" } },
  { "ar", { "ar", "ara", 1, "Arabic" } },
  { "as", { "as", "asm", -1, "Assamese" } },
  { "av", { "av", "ava", -1, "Avaric" } },
  { "ay", { "ay", "aym", -1, "Aymara" } },
  { "az", { "az", "aze", 0, "Azerbaijani" } },
  { "ba", { "ba", "bak", 0, "Bashkir" } },
  { "be", { "be", "bel", 0, "Belarusian" } },
  { "bg", { "bg", "bul", 0, "Bulgarian" } },
  { "bh", { "bh", "bih", -1, "Bihari" } },
  { "bi", { "bi", "bis", -1, "Bislama" } },
  { "bm", { "bm", "bam", -1, "Bambara" } },
  { "bn", { "bn", "ben", -1, "Bengali" } },
  { "bo", { "bo", "tib", -1, "Tibetan" } },
  { "br", { "br", "bre", -1, "Breton" } },
  { "bs", { "bs", "bos", 0, "Bosnian" } },
  { "ca", { "ca", "cat", -1, "Catalan" } },
  { "ce", { "ce", "che", -1, "Chechen" } },
  { "ch", { "ch", "cha", -1, "Chamorro" } },
  { "co", { "co", "cos", -1, "Corsican" } },
  { "cr", { "cr", "cre", -1, "Cree" } },
  { "cs", { "cs", "cze", 0, "Czech" } },
  { "cu", { "cu", "chu", 0, "Church Slavic" } },
  { "cv", { "cv", "chv", 0, "Chuvash" } },
  { "cy", { "cy", "wel", 0, "Welsh" } },
  { "da", { "da", "dan", 0, "Danish" } },
  { "de", { "de", "ger", 0, "German" } },
  { "dv", { "dv", "div", -1, "Divehi" } },
  { "dz", { "dz", "dzo", -1, "Dzongkha" } },
  { "ee", { "ee", "ewe", -1, "Ewe" } },
  { "el", { "el", "gre", 0, "Greek" } },
  { "en", { "en", "eng", 0, "English" } },
  { "eo", { "eo", "epo", 0, "Esperanto" } },
  { "es", { "es", "spa", 0, "Spanish" } },
  { "et", { "et", "est", 0, "Estonian" } },
  { "eu", { "eu", "baq", 0, "Basque" } },
  { "fa", { "fa", "per", -1, "Persian" } },
  { "ff", { "ff", "ful", -1, "Fulah" } },
  { "fi", { "fi", "fin", 0, "Finnish" } },
  { "fj", { "fj", "fij", -1, "Fijian" } },
  { "fo", { "fo", "fao", -1, "Faroese" } },
  { "fr", { "fr", "fre", 0, "French" } },
  { "fy", { "fy", "fry", -1, "Western Frisian" } },
  { "ga", { "ga", "gle", 0, "Irish" } },
  { "gd", { "gd", "gla", 0, "Scottish Gaelic" } },
  { "gl", { "gl", "glg", -1, "Galician" } },
  { "gn", { "gn", "grn", -1, "Guarani" } },
  { "gu", { "gu", "guj", -1, "Gujarati" } },
  { "gv", { "gv", "glv", -1, "Manx" } },
  { "ha", { "ha", "hau", -1, "Hausa" } },
  { "he", { "he", "heb", 1, "Hebrew" } },
  { "hi", { "hi", "hin", -1, "Hindi" } },
  { "ho", { "ho", "hmo", -1, "Hiri Motu" } },
  { "hr", { "hr", "hrv", 0, "Croatian" } },
  { "ht", { "ht", "hat", -1, "Haitian" } },
  { "hu", { "hu", "hun", 0, "Hungarian" } },
  { "hy", { "hy", "arm", 0, "Armenian" } },
  { "hz", { "hz", "her", -1, "Herero" } },
  { "ia", { "ia", "ina", -1, "Interlingua" } },
  { "id", { "id", "ind", -1, "Indonesian" } },
  { "ie", { "ie", "ile", -1, "Interlingue" } },
  { "ig", { "ig", "ibo", -1, "Igbo" } },
  { "ii", { "ii", "iii", -1, "Sichuan Yi" } },
  { "ik", { "ik", "ipk", -1, "Inupiaq" } },
  { "io", { "io", "ido", -1, "Ido" } },
  { "is", { "is", "ice", -1, "Icelandic" } },
  { "it", { "it", "ita", 0, "Italian" } },
  { "iu", { "iu", "iku", -1, "Inuktitut" } },
  { "ja", { "ja", "jpn", 0, "Japanese" } },
  { "jv", { "jv", "jav", -1, "Javanese" } },
  { "ka", { "ka", "geo", 0, "Georgian" } },
  { "kg", { "kg", "kon", -1, "Kongo" } },
  { "ki", { "ki", "kik", -1, "Kikuyu" } },
  { "kj", { "kj", "kua", -1, "Kwanyama" } },
  { "kk", { "kk", "kaz", 0, "Kazakh" } },
  { "kl", { "kl", "kal", -1, "Kalaallisut" } },
  { "km", { "km", "khm", -1, "Khmer" } },
  { "kn", { "kn", "kan", -1, "Kannada" } },
  { "ko", { "ko", "kor", 0, "Korean" } },
  { "kr", { "kr", "kau", -1, "Kanuri" } },
  { "ks", { "ks", "kas", -1, "Kashmiri" } },
  { "ku", { "ku", "kur", -1, "Kurdish" } },
  { "kv", { "kv", "kom", 0, "Komi" } },
  { "kw", { "kw", "cor", -1, "Cornish" } },
  { "ky", { "ky", "kir", -1, "Kirghiz" } },
  { "la", { "la", "lat", 0, "Latin" } },
  { "lb", { "lb", "ltz", 0, "Luxembourgish" } },
  { "lg", { "lg", "lug", -1, "Ganda" } },
  { "li", { "li", "lim", -1, "Limburgish" } },
  { "ln", { "ln", "lin", -1, "Lingala" } },
  { "lo", { "lo", "lao", -1, "Lao" } },
  { "lt", { "lt", "lit", 0, "Lithuanian" } },
  { "lu", { "lu", "lub", -1, "Luba-Katanga" } },
  { "lv", { "lv", "lav", 0, "Latvian" } },
  { "mg", { "mg", "mlg", -1, "Malagasy" } },
  { "mh", { "mh", "mah", -1, "Marshallese" } },
  { "mi", { "mi", "mao", -1, "Maori" } },
  { "mk", { "mk", "mac", 0, "Macedonian" } },
  { "ml", { "ml", "mal", -1, "Malayalam" } },
  { "mn", { "mn", "mon", -1, "Mongolian" } },
  { "mr", { "mr", "mar", -1, "Marathi" } },
  { "ms", { "ms", "may", -1, "Malay" } },
  { "mt", { "mt", "mlt", -1, "Maltese" } },
  { "my", { "my", "bur", -1, "Burmese" } },
  { "na", { "na", "nau", -1, "Nauru" } },
  { "nb", { "nb", "nob", 0, "Norwegian Bokmal" } },
  { "nd", { "nd", "nde", -1, "North Ndebele" } },
  { "ne", { "ne", "nep", -1, "Nepali" } },
  { "ng", { "ng", "ndo", -1, "Ndonga" } },
  { "nl", { "nl", "dut", -1, "Dutch" } },
  { "nn", { "nn", "nno", -1, "Norwegian Nynorsk" } },
  { "no", { "no", "nor", 0, "Norwegian" } },
  { "nr", { "nr", "nbl", -1, "South Ndebele" } },
  { "nv", { "nv", "nav", -1, "Navajo" } },
  { "ny", { "ny", "nya", -1, "Chichewa" } },
  { "oc", { "oc", "oci", -1, "Occitan" } },
  { "oj", { "oj", "oji", -1, "Ojibwa" } },
  { "om", { "om", "orm", -1, "Oromo" } },
  { "or", { "or", "ori", -1, "Oriya" } },
  { "os", { "os", "oss", -1, "Ossetian" } },
  { "pa", { "pa", "pan", -1, "Panjabi" } },
  { "pi", { "pi", "pli", -1, "Pali" } },
  { "pl", { "pl", "pol", 0, "Polish" } },
  { "ps", { "ps", "pus", -1, "Pashto" } },
  { "pt", { "pt", "por", 0, "Portuguese" } },
  { "qu", { "qu", "que", -1, "Quechua" } },
  { "rm", { "rm", "roh", -1, "Raeto-Romance" } },
  { "rn", { "rn", "run", -1, "Kirundi" } },
  { "ro", { "ro", "rum", 0, "Romanian" } },
  { "ru", { "ru", "rus", 0, "Russian" } },
  { "rw", { "rw", "kin", -1, "Kinyarwanda" } },
  { "sa", { "sa", "san", -1, "Sanskrit" } },
  { "sc", { "sc", "srd", -1, "Sardinian" } },
  { "sd", { "sd", "snd", -1, "Sindhi" } },
  { "se", { "se", "sme", -1, "Northern Sami" } },
  { "sg", { "sg", "sag", -1, "Sango" } },
  { "sh", { "sh", "shr", 0, "Serbo-Croatian" } },
  { "si", { "si", "sin", -1, "Sinhala" } },
  { "sk", { "sk", "slo", 0, "Slovak" } },
  { "sl", { "sl", "slv", 0, "Slovenian" } },
  { "sm", { "sm", "smo", -1, "Samoan" } },
  { "sn", { "sn", "sna", -1, "Shona" } },
  { "so", { "so", "som", -1, "Somali" } },
  { "sq", { "sq", "alb", 0, "Albanian" } },
  { "sr", { "sr", "srp", 0, "Serbian" } },
  { "ss", { "ss", "ssw", -1, "Swati" } },
  { "st", { "st", "sot", -1, "Southern Sotho" } },
  { "su", { "su", "sun", -1, "Sundanese" } },
  { "sv", { "sv", "swe", 0, "Swedish" } },
  { "sw", { "sw", "swa", -1, "Swahili" } },
  { "ta", { "ta", "tam", -1, "Tamil" } },
  { "te", { "te", "tel", -1, "Telugu" } },
  { "tg", { "tg", "tgk", 0, "Tajik" } },
  { "th", { "th", "tha", -1, "Thai" } },
  { "ti", { "ti", "tir", -1, "Tigrinya" } },
  { "tk", { "tk", "tuk", 0, "Turkmen" } },
  { "tl", { "tl", "tgl", -1, "Tagalog" } },
  { "tn", { "tn", "tsn", -1, "Tswana" } },
  { "to", { "to", "ton", -1, "Tonga" } },
  { "tr", { "tr", "tur", 0, "Turkish" } },
  { "ts", { "ts", "tso", -1, "Tsonga" } },
  { "tt", { "tt", "tat", -1, "Tatar" } },
  { "tw", { "tw", "twi", -1, "Twi" } },
  { "ty", { "ty", "tah", -1, "Tahitian" } },
  { "ug", { "ug", "uig", -1, "Uighur" } },
  { "uk", { "uk", "ukr", -1, "Ukrainian" } },
  { "ur", { "ur", "urd", -1, "Urdu" } },
  { "uz", { "uz", "uzb", 0, "Uzbek" } },
  { "ve", { "ve", "ven", -1, "Venda" } },
  { "vi", { "vi", "vie", -1, "Vietnamese" } },
  { "vo", { "vo", "vol", 0, "Volapuk" } },
  { "wa", { "wa", "wln", -1, "Walloon" } },
  { "wo", { "wo", "wol", -1, "Wolof" } },
  { "xh", { "xh", "xho", -1, "Xhosa" } },
  { "yi", { "yi", "yid", -1, "Yiddish" } },
  { "yo", { "yo", "yor", -1, "Yoruba" } },
  { "za", { "za", "zha", -1, "Zhuang" } },
  { "zh", { "zh", "chi", 0, "Chinese" } },
  { "zu", { "zu", "zul", -1, "Zulu" } },
  { "jb", { "jb", "jbo", 0, "Lojban" } },
};

QString LangCoder::decode( quint32 _code )
{
  if ( auto code = intToCode2( _code ); code2Exists( code ) )
    return QString::fromStdString( LANG_CODE_MAP[ code ].lang );

  return {};
}
bool LangCoder::code2Exists( const QString & _code )
{
  return LANG_CODE_MAP.contains( _code );
}

QIcon LangCoder::icon( quint32 _code )
{
  if ( auto code = intToCode2( _code ); code2Exists( code ) ) {
    const GDLangCode & lc = LANG_CODE_MAP[ code ];
    return QIcon( ":/flags/" + QString( lc.code2 ) + ".png" );
  }

  return {};
}

QString LangCoder::intToCode2( quint32 val )
{
  if ( !val || val == 0xFFffFFff )
    return {};

  QByteArray ba;
  ba.append( val & 0xFF );
  ba.append( ( val >> 8 ) & 0xFF );

  return QString::fromLatin1( ba );
}

quint32 LangCoder::findIdForLanguage( gd::wstring const & lang )
{
  const auto langFolded = Utf8::encode( lang );

  for ( auto const & lc : LANG_CODE_MAP ) {
    if ( strcasecmp( langFolded.c_str(), lc.lang.c_str() ) == 0 ) {
      return code2toInt( lc.code2.toStdString().c_str() );
    }
  }

  return Language::findBlgLangIDByEnglishName( lang );
}

quint32 LangCoder::findIdForLanguageCode3( std::string const & code )
{
  for ( auto const & lc : LANG_CODE_MAP ) {
    if ( code == lc.code3 ) {
      return code2toInt( lc.code2 );
    }
  }

  return 0;
}

quint32 LangCoder::guessId( const QString & lang )
{
  QString lstr = lang.simplified().toLower();

  // too small to guess
  if ( lstr.size() < 2 )
    return 0;

  // check if it could be the whole language name
  if ( lstr.size() >= 3 ) {
    for ( auto const & lc : LANG_CODE_MAP ) {
      if ( lstr == ( lstr.size() == 3 ? QString::fromStdString( lc.code3 ) : QString::fromStdString( lc.lang ) ) ) {
        return code2toInt( lc.code2 );
      }
    }
  }

  // still not found - try to match by 2-symbol code
  return code2toInt( lstr.left( 2 ).toLatin1().data() );
}


std::pair< quint32, quint32 > LangCoder::findLangIdPairFromName( QString const & name )
{
  static QRegularExpression reg( "(?=([a-z]{2,3})-([a-z]{2,3}))", QRegularExpression::CaseInsensitiveOption );

  auto matches = reg.globalMatch( name );
  while ( matches.hasNext() ) {
    auto m = matches.next();

    auto fromId = guessId( m.captured( 1 ).toLower() );
    auto toId   = guessId( m.captured( 2 ).toLower() );

    if ( code2Exists( intToCode2( fromId ) ) && code2Exists( intToCode2( toId ) ) ) {
      return { fromId, toId };
    }
  }
  return { 0, 0 };
}

std::pair< quint32, quint32 > LangCoder::findLangIdPairFromPath( std::string const & p )
{
  return findLangIdPairFromName( QFileInfo( QString::fromStdString( p ) ).fileName() );
}

bool LangCoder::isLanguageRTL( quint32 _code )
{
  if ( auto code = intToCode2( _code ); code2Exists( code ) ) {
    GDLangCode lc = LANG_CODE_MAP[ code ];
    if ( lc.isRTL < 0 ) {
      lc.isRTL = static_cast< int >( QLocale( lc.code2 ).textDirection() == Qt::RightToLeft );
    }
    return lc.isRTL != 0;
  }

  return false;
}
