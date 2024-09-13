#include "language.hh"
#include "langcoder.hh"
#include <map>
#include <QMap>
#include <QCoreApplication>

namespace Language {

namespace {

using std::map;

struct LangData
{
  QString english;
  QString localized;
  QString country;
};

struct Db
{
  static Db const & instance();

  [[nodiscard]] QMap< QString, LangData > const & getIso2ToLangData() const
  {
    return iso2LangData;
  }

  [[nodiscard]] QMap< QString, QString > const & locale2LanguageMap() const
  {
    return localeLanguage;
  }

private:

  QMap< QString, LangData > iso2LangData;

  QMap< QString, QString > localeLanguage = { { "fr_FR", QT_TR_NOOP( "French" ) },
                                              { "es_ES", QT_TR_NOOP( "Spanish" ) },
                                              { "be_BY", QT_TR_NOOP( "Belarusian" ) },
                                              { "bg_BG", QT_TR_NOOP( "Bulgarian" ) },
                                              { "cs_CZ", QT_TR_NOOP( "Czech" ) },
                                              { "de_DE", QT_TR_NOOP( "German" ) },
                                              { "el_GR", QT_TR_NOOP( "Greek" ) },
                                              { "fi_FI", QT_TR_NOOP( "Finnish" ) },
                                              { "it_IT", QT_TR_NOOP( "Italian" ) },
                                              { "ja_JP", QT_TR_NOOP( "Japanese" ) },
                                              { "ko_KR", QT_TR_NOOP( "Korean" ) },
                                              { "lt_LT", QT_TR_NOOP( "Lithuanian" ) },
                                              { "mk_MK", QT_TR_NOOP( "Macedonian" ) },
                                              { "nl_NL", QT_TR_NOOP( "Dutch" ) },
                                              { "pl_PL", QT_TR_NOOP( "Polish" ) },
                                              { "pt_PT", QT_TR_NOOP( "Portuguese" ) },
                                              { "ru_RU", QT_TR_NOOP( "Russian" ) },
                                              { "sk_SK", QT_TR_NOOP( "Slovak" ) },
                                              { "sq_AL", QT_TR_NOOP( "Albanian" ) },
                                              { "sr_SP", QT_TR_NOOP( "Serbian (Cyrillic)" ) },
                                              { "sv_SE", QT_TR_NOOP( "Swedish" ) },
                                              { "tr_TR", QT_TR_NOOP( "Turkish" ) },
                                              { "uk_UA", QT_TR_NOOP( "Ukrainian" ) },
                                              { "zh_CN", QT_TR_NOOP( "Chinese Simplified" ) },
                                              { "zh_TW", QT_TR_NOOP( "Chinese Traditional" ) },
                                              { "vi_VN", QT_TR_NOOP( "Vietnamese" ) },
                                              { "pt_BR", QT_TR_NOOP( "Portuguese, Brazilian" ) },
                                              { "fa_IR", QT_TR_NOOP( "Persian" ) },
                                              { "es_AR", QT_TR_NOOP( "Spanish, Argentina" ) },
                                              { "hi_IN", QT_TR_NOOP( "Hindi" ) },
                                              { "eo_UY", QT_TR_NOOP( "Esperanto" ) },
                                              { "de_CH", QT_TR_NOOP( "German, Switzerland" ) },
                                              { "es_BO", QT_TR_NOOP( "Spanish, Bolivia" ) },
                                              { "tg_TJ", QT_TR_NOOP( "Tajik" ) },
                                              { "qu_PE", QT_TR_NOOP( "Quechua" ) },
                                              { "ay_BO", QT_TR_NOOP( "Aymara" ) },
                                              { "ar_SA", QT_TR_NOOP( "Arabic, Saudi Arabia" ) },
                                              { "tk_TM", QT_TR_NOOP( "Turkmen" ) },
                                              { "ie_001", QT_TR_NOOP( "Interlingue" ) },
                                              { "jbo_EN", QT_TR_NOOP( "Lojban" ) },
                                              { "hu_HU", QT_TR_NOOP( "Hungarian" ) },
                                              { "en_US", QT_TR_NOOP( "English" ) } };

  Db();

  void addEntry( QString const & iso2, QString const & english, QString const & localized );

  void addExtraCountry( QString const & iso2, QString const & country );
};

Db const & Db::instance()
{
  static Db v;

  return v;
}

void Db::addEntry( QString const & iso2, QString const & english, QString const & localized )
{
  LangData lang_data;
  lang_data.english   = english;
  lang_data.localized = localized;
  iso2LangData.insert( iso2, lang_data );
}

void Db::addExtraCountry( QString const & iso2, QString const & country )
{
  if ( !iso2LangData.contains( iso2 ) ) {
    return;
  }
  iso2LangData[ iso2 ].country = country;
}

Db::Db()
{
  addEntry( "aa", "Afar", QCoreApplication::translate( "Language", "Afar" ) );
  addEntry( "ab", "Abkhazian", QCoreApplication::translate( "Language", "Abkhazian" ) );
  addEntry( "ae", "Avestan", QCoreApplication::translate( "Language", "Avestan" ) );
  addEntry( "af", "Afrikaans", QCoreApplication::translate( "Language", "Afrikaans" ) );
  addEntry( "ak", "Akan", QCoreApplication::translate( "Language", "Akan" ) );
  addEntry( "am", "Amharic", QCoreApplication::translate( "Language", "Amharic" ) );
  addEntry( "an", "Aragonese", QCoreApplication::translate( "Language", "Aragonese" ) );
  addEntry( "ar", "Arabic", QCoreApplication::translate( "Language", "Arabic" ) );
  addEntry( "as", "Assamese", QCoreApplication::translate( "Language", "Assamese" ) );
  addEntry( "av", "Avaric", QCoreApplication::translate( "Language", "Avaric" ) );
  addEntry( "ay", "Aymara", QCoreApplication::translate( "Language", "Aymara" ) );
  addEntry( "az", "Azerbaijani", QCoreApplication::translate( "Language", "Azerbaijani" ) );
  addEntry( "ba", "Bashkir", QCoreApplication::translate( "Language", "Bashkir" ) );
  addEntry( "be", "Belarusian", QCoreApplication::translate( "Language", "Belarusian" ) );
  addEntry( "bg", "Bulgarian", QCoreApplication::translate( "Language", "Bulgarian" ) );
  addEntry( "bh", "Bihari", QCoreApplication::translate( "Language", "Bihari" ) );
  addEntry( "bi", "Bislama", QCoreApplication::translate( "Language", "Bislama" ) );
  addEntry( "bm", "Bambara", QCoreApplication::translate( "Language", "Bambara" ) );
  addEntry( "bn", "Bengali", QCoreApplication::translate( "Language", "Bengali" ) );
  addEntry( "bo", "Tibetan", QCoreApplication::translate( "Language", "Tibetan" ) );
  addEntry( "br", "Breton", QCoreApplication::translate( "Language", "Breton" ) );
  addEntry( "bs", "Bosnian", QCoreApplication::translate( "Language", "Bosnian" ) );
  addEntry( "ca", "Catalan", QCoreApplication::translate( "Language", "Catalan" ) );
  addEntry( "ce", "Chechen", QCoreApplication::translate( "Language", "Chechen" ) );
  addEntry( "ch", "Chamorro", QCoreApplication::translate( "Language", "Chamorro" ) );
  addEntry( "co", "Corsican", QCoreApplication::translate( "Language", "Corsican" ) );
  addEntry( "cr", "Cree", QCoreApplication::translate( "Language", "Cree" ) );
  addEntry( "cs", "Czech", QCoreApplication::translate( "Language", "Czech" ) );
  addEntry( "cu", "Church Slavic", QCoreApplication::translate( "Language", "Church Slavic" ) );
  addEntry( "cv", "Chuvash", QCoreApplication::translate( "Language", "Chuvash" ) );
  addEntry( "cy", "Welsh", QCoreApplication::translate( "Language", "Welsh" ) );
  addEntry( "da", "Danish", QCoreApplication::translate( "Language", "Danish" ) );
  addEntry( "de", "German", QCoreApplication::translate( "Language", "German" ) );
  addEntry( "dv", "Divehi", QCoreApplication::translate( "Language", "Divehi" ) );
  addEntry( "dz", "Dzongkha", QCoreApplication::translate( "Language", "Dzongkha" ) );
  addEntry( "ee", "Ewe", QCoreApplication::translate( "Language", "Ewe" ) );
  addEntry( "el", "Greek", QCoreApplication::translate( "Language", "Greek" ) );
  addEntry( "en", "English", QCoreApplication::translate( "Language", "English" ) );
  addEntry( "eo", "Esperanto", QCoreApplication::translate( "Language", "Esperanto" ) );
  addEntry( "es", "Spanish", QCoreApplication::translate( "Language", "Spanish" ) );
  addEntry( "et", "Estonian", QCoreApplication::translate( "Language", "Estonian" ) );
  addEntry( "eu", "Basque", QCoreApplication::translate( "Language", "Basque" ) );
  addEntry( "fa", "Persian", QCoreApplication::translate( "Language", "Persian" ) );
  addEntry( "ff", "Fulah", QCoreApplication::translate( "Language", "Fulah" ) );
  addEntry( "fi", "Finnish", QCoreApplication::translate( "Language", "Finnish" ) );
  addEntry( "fj", "Fijian", QCoreApplication::translate( "Language", "Fijian" ) );
  addEntry( "fo", "Faroese", QCoreApplication::translate( "Language", "Faroese" ) );
  addEntry( "fr", "French", QCoreApplication::translate( "Language", "French" ) );
  addEntry( "fy", "Western Frisian", QCoreApplication::translate( "Language", "Western Frisian" ) );
  addEntry( "ga", "Irish", QCoreApplication::translate( "Language", "Irish" ) );
  addEntry( "gd", "Scottish Gaelic", QCoreApplication::translate( "Language", "Scottish Gaelic" ) );
  addEntry( "gl", "Galician", QCoreApplication::translate( "Language", "Galician" ) );
  addEntry( "gn", "Guarani", QCoreApplication::translate( "Language", "Guarani" ) );
  addEntry( "gu", "Gujarati", QCoreApplication::translate( "Language", "Gujarati" ) );
  addEntry( "gv", "Manx", QCoreApplication::translate( "Language", "Manx" ) );
  addEntry( "ha", "Hausa", QCoreApplication::translate( "Language", "Hausa" ) );
  addEntry( "he", "Hebrew", QCoreApplication::translate( "Language", "Hebrew" ) );
  addEntry( "hi", "Hindi", QCoreApplication::translate( "Language", "Hindi" ) );
  addEntry( "ho", "Hiri Motu", QCoreApplication::translate( "Language", "Hiri Motu" ) );
  addEntry( "hr", "Croatian", QCoreApplication::translate( "Language", "Croatian" ) );
  addEntry( "ht", "Haitian", QCoreApplication::translate( "Language", "Haitian" ) );
  addEntry( "hu", "Hungarian", QCoreApplication::translate( "Language", "Hungarian" ) );
  addEntry( "hy", "Armenian", QCoreApplication::translate( "Language", "Armenian" ) );
  addEntry( "hz", "Herero", QCoreApplication::translate( "Language", "Herero" ) );
  addEntry( "ia", "Interlingua", QCoreApplication::translate( "Language", "Interlingua" ) );
  addEntry( "id", "Indonesian", QCoreApplication::translate( "Language", "Indonesian" ) );
  addEntry( "ie", "Interlingue", QCoreApplication::translate( "Language", "Interlingue" ) );
  addEntry( "ig", "Igbo", QCoreApplication::translate( "Language", "Igbo" ) );
  addEntry( "ii", "Sichuan Yi", QCoreApplication::translate( "Language", "Sichuan Yi" ) );
  addEntry( "ik", "Inupiaq", QCoreApplication::translate( "Language", "Inupiaq" ) );
  addEntry( "io", "Ido", QCoreApplication::translate( "Language", "Ido" ) );
  addEntry( "is", "Icelandic", QCoreApplication::translate( "Language", "Icelandic" ) );
  addEntry( "it", "Italian", QCoreApplication::translate( "Language", "Italian" ) );
  addEntry( "iu", "Inuktitut", QCoreApplication::translate( "Language", "Inuktitut" ) );
  addEntry( "ja", "Japanese", QCoreApplication::translate( "Language", "Japanese" ) );
  addEntry( "jv", "Javanese", QCoreApplication::translate( "Language", "Javanese" ) );
  addEntry( "ka", "Georgian", QCoreApplication::translate( "Language", "Georgian" ) );
  addEntry( "kg", "Kongo", QCoreApplication::translate( "Language", "Kongo" ) );
  addEntry( "ki", "Kikuyu", QCoreApplication::translate( "Language", "Kikuyu" ) );
  addEntry( "kj", "Kwanyama", QCoreApplication::translate( "Language", "Kwanyama" ) );
  addEntry( "kk", "Kazakh", QCoreApplication::translate( "Language", "Kazakh" ) );
  addEntry( "kl", "Kalaallisut", QCoreApplication::translate( "Language", "Kalaallisut" ) );
  addEntry( "km", "Khmer", QCoreApplication::translate( "Language", "Khmer" ) );
  addEntry( "kn", "Kannada", QCoreApplication::translate( "Language", "Kannada" ) );
  addEntry( "ko", "Korean", QCoreApplication::translate( "Language", "Korean" ) );
  addEntry( "kr", "Kanuri", QCoreApplication::translate( "Language", "Kanuri" ) );
  addEntry( "ks", "Kashmiri", QCoreApplication::translate( "Language", "Kashmiri" ) );
  addEntry( "ku", "Kurdish", QCoreApplication::translate( "Language", "Kurdish" ) );
  addEntry( "kv", "Komi", QCoreApplication::translate( "Language", "Komi" ) );
  addEntry( "kw", "Cornish", QCoreApplication::translate( "Language", "Cornish" ) );
  addEntry( "ky", "Kirghiz", QCoreApplication::translate( "Language", "Kirghiz" ) );
  addEntry( "la", "Latin", QCoreApplication::translate( "Language", "Latin" ) );
  addEntry( "lb", "Luxembourgish", QCoreApplication::translate( "Language", "Luxembourgish" ) );
  addEntry( "lg", "Ganda", QCoreApplication::translate( "Language", "Ganda" ) );
  addEntry( "li", "Limburgish", QCoreApplication::translate( "Language", "Limburgish" ) );
  addEntry( "ln", "Lingala", QCoreApplication::translate( "Language", "Lingala" ) );
  addEntry( "lo", "Lao", QCoreApplication::translate( "Language", "Lao" ) );
  addEntry( "lt", "Lithuanian", QCoreApplication::translate( "Language", "Lithuanian" ) );
  addEntry( "lu", "Luba-Katanga", QCoreApplication::translate( "Language", "Luba-Katanga" ) );
  addEntry( "lv", "Latvian", QCoreApplication::translate( "Language", "Latvian" ) );
  addEntry( "mg", "Malagasy", QCoreApplication::translate( "Language", "Malagasy" ) );
  addEntry( "mh", "Marshallese", QCoreApplication::translate( "Language", "Marshallese" ) );
  addEntry( "mi", "Maori", QCoreApplication::translate( "Language", "Maori" ) );
  addEntry( "mk", "Macedonian", QCoreApplication::translate( "Language", "Macedonian" ) );
  addEntry( "ml", "Malayalam", QCoreApplication::translate( "Language", "Malayalam" ) );
  addEntry( "mn", "Mongolian", QCoreApplication::translate( "Language", "Mongolian" ) );
  addEntry( "mr", "Marathi", QCoreApplication::translate( "Language", "Marathi" ) );
  addEntry( "ms", "Malay", QCoreApplication::translate( "Language", "Malay" ) );
  addEntry( "mt", "Maltese", QCoreApplication::translate( "Language", "Maltese" ) );
  addEntry( "my", "Burmese", QCoreApplication::translate( "Language", "Burmese" ) );
  addEntry( "na", "Nauru", QCoreApplication::translate( "Language", "Nauru" ) );
  addEntry( "nb", "Norwegian Bokmal", QCoreApplication::translate( "Language", "Norwegian Bokmal" ) );
  addEntry( "nd", "North Ndebele", QCoreApplication::translate( "Language", "North Ndebele" ) );
  addEntry( "ne", "Nepali", QCoreApplication::translate( "Language", "Nepali" ) );
  addEntry( "ng", "Ndonga", QCoreApplication::translate( "Language", "Ndonga" ) );
  addEntry( "nl", "Dutch", QCoreApplication::translate( "Language", "Dutch" ) );
  addEntry( "nn", "Norwegian Nynorsk", QCoreApplication::translate( "Language", "Norwegian Nynorsk" ) );
  addEntry( "no", "Norwegian", QCoreApplication::translate( "Language", "Norwegian" ) );
  addEntry( "nr", "South Ndebele", QCoreApplication::translate( "Language", "South Ndebele" ) );
  addEntry( "nv", "Navajo", QCoreApplication::translate( "Language", "Navajo" ) );
  addEntry( "ny", "Chichewa", QCoreApplication::translate( "Language", "Chichewa" ) );
  addEntry( "oc", "Occitan", QCoreApplication::translate( "Language", "Occitan" ) );
  addEntry( "oj", "Ojibwa", QCoreApplication::translate( "Language", "Ojibwa" ) );
  addEntry( "om", "Oromo", QCoreApplication::translate( "Language", "Oromo" ) );
  addEntry( "or", "Oriya", QCoreApplication::translate( "Language", "Oriya" ) );
  addEntry( "os", "Ossetian", QCoreApplication::translate( "Language", "Ossetian" ) );
  addEntry( "pa", "Panjabi", QCoreApplication::translate( "Language", "Panjabi" ) );
  addEntry( "pi", "Pali", QCoreApplication::translate( "Language", "Pali" ) );
  addEntry( "pl", "Polish", QCoreApplication::translate( "Language", "Polish" ) );
  addEntry( "ps", "Pashto", QCoreApplication::translate( "Language", "Pashto" ) );
  addEntry( "pt", "Portuguese", QCoreApplication::translate( "Language", "Portuguese" ) );
  addEntry( "qu", "Quechua", QCoreApplication::translate( "Language", "Quechua" ) );
  addEntry( "rm", "Raeto-Romance", QCoreApplication::translate( "Language", "Raeto-Romance" ) );
  addEntry( "rn", "Kirundi", QCoreApplication::translate( "Language", "Kirundi" ) );
  addEntry( "ro", "Romanian", QCoreApplication::translate( "Language", "Romanian" ) );
  addEntry( "ru", "Russian", QCoreApplication::translate( "Language", "Russian" ) );
  addEntry( "rw", "Kinyarwanda", QCoreApplication::translate( "Language", "Kinyarwanda" ) );
  addEntry( "sa", "Sanskrit", QCoreApplication::translate( "Language", "Sanskrit" ) );
  addEntry( "sc", "Sardinian", QCoreApplication::translate( "Language", "Sardinian" ) );
  addEntry( "sd", "Sindhi", QCoreApplication::translate( "Language", "Sindhi" ) );
  addEntry( "se", "Northern Sami", QCoreApplication::translate( "Language", "Northern Sami" ) );
  addEntry( "sg", "Sango", QCoreApplication::translate( "Language", "Sango" ) );
  addEntry( "sh", "Serbo-Croatian", QCoreApplication::translate( "Language", "Serbo-Croatian" ) );
  addEntry( "si", "Sinhala", QCoreApplication::translate( "Language", "Sinhala" ) );
  addEntry( "sk", "Slovak", QCoreApplication::translate( "Language", "Slovak" ) );
  addEntry( "sl", "Slovenian", QCoreApplication::translate( "Language", "Slovenian" ) );
  addEntry( "sm", "Samoan", QCoreApplication::translate( "Language", "Samoan" ) );
  addEntry( "sn", "Shona", QCoreApplication::translate( "Language", "Shona" ) );
  addEntry( "so", "Somali", QCoreApplication::translate( "Language", "Somali" ) );
  addEntry( "sq", "Albanian", QCoreApplication::translate( "Language", "Albanian" ) );
  addEntry( "sr", "Serbian", QCoreApplication::translate( "Language", "Serbian" ) );
  addEntry( "ss", "Swati", QCoreApplication::translate( "Language", "Swati" ) );
  addEntry( "st", "Southern Sotho", QCoreApplication::translate( "Language", "Southern Sotho" ) );
  addEntry( "su", "Sundanese", QCoreApplication::translate( "Language", "Sundanese" ) );
  addEntry( "sv", "Swedish", QCoreApplication::translate( "Language", "Swedish" ) );
  addEntry( "sw", "Swahili", QCoreApplication::translate( "Language", "Swahili" ) );
  addEntry( "ta", "Tamil", QCoreApplication::translate( "Language", "Tamil" ) );
  addEntry( "te", "Telugu", QCoreApplication::translate( "Language", "Telugu" ) );
  addEntry( "tg", "Tajik", QCoreApplication::translate( "Language", "Tajik" ) );
  addEntry( "th", "Thai", QCoreApplication::translate( "Language", "Thai" ) );
  addEntry( "ti", "Tigrinya", QCoreApplication::translate( "Language", "Tigrinya" ) );
  addEntry( "tk", "Turkmen", QCoreApplication::translate( "Language", "Turkmen" ) );
  addEntry( "tl", "Tagalog", QCoreApplication::translate( "Language", "Tagalog" ) );
  addEntry( "tn", "Tswana", QCoreApplication::translate( "Language", "Tswana" ) );
  addEntry( "to", "Tonga", QCoreApplication::translate( "Language", "Tonga" ) );
  addEntry( "tr", "Turkish", QCoreApplication::translate( "Language", "Turkish" ) );
  addEntry( "ts", "Tsonga", QCoreApplication::translate( "Language", "Tsonga" ) );
  addEntry( "tt", "Tatar", QCoreApplication::translate( "Language", "Tatar" ) );
  addEntry( "tw", "Twi", QCoreApplication::translate( "Language", "Twi" ) );
  addEntry( "ty", "Tahitian", QCoreApplication::translate( "Language", "Tahitian" ) );
  addEntry( "ug", "Uighur", QCoreApplication::translate( "Language", "Uighur" ) );
  addEntry( "uk", "Ukrainian", QCoreApplication::translate( "Language", "Ukrainian" ) );
  addEntry( "ur", "Urdu", QCoreApplication::translate( "Language", "Urdu" ) );
  addEntry( "uz", "Uzbek", QCoreApplication::translate( "Language", "Uzbek" ) );
  addEntry( "ve", "Venda", QCoreApplication::translate( "Language", "Venda" ) );
  addEntry( "vi", "Vietnamese", QCoreApplication::translate( "Language", "Vietnamese" ) );
  addEntry( "vo", "Volapuk", QCoreApplication::translate( "Language", "Volapuk" ) );
  addEntry( "wa", "Walloon", QCoreApplication::translate( "Language", "Walloon" ) );
  addEntry( "wo", "Wolof", QCoreApplication::translate( "Language", "Wolof" ) );
  addEntry( "xh", "Xhosa", QCoreApplication::translate( "Language", "Xhosa" ) );
  addEntry( "yi", "Yiddish", QCoreApplication::translate( "Language", "Yiddish" ) );
  addEntry( "yo", "Yoruba", QCoreApplication::translate( "Language", "Yoruba" ) );
  addEntry( "za", "Zhuang", QCoreApplication::translate( "Language", "Zhuang" ) );
  addEntry( "zh", "Chinese", QCoreApplication::translate( "Language", "Chinese" ) );
  addEntry( "zu", "Zulu", QCoreApplication::translate( "Language", "Zulu" ) );
  addEntry( "jb", "Lojban", QCoreApplication::translate( "Language", "Lojban" ) );

  // Countries

  addExtraCountry( "aa", "et" );
  addExtraCountry( "af", "za" );
  addExtraCountry( "am", "et" );
  addExtraCountry( "an", "es" );
  addExtraCountry( "ar", "ae" );
  addExtraCountry( "as", "in" );
  addExtraCountry( "az", "az" );
  addExtraCountry( "be", "by" );
  addExtraCountry( "bg", "bg" );
  addExtraCountry( "bn", "bd" );
  addExtraCountry( "bo", "cn" );
  addExtraCountry( "br", "fr" );
  addExtraCountry( "bs", "ba" );
  addExtraCountry( "ca", "ad" );
  addExtraCountry( "cs", "cz" );
  addExtraCountry( "cy", "gb" );
  addExtraCountry( "da", "dk" );
  addExtraCountry( "de", "de" );
  addExtraCountry( "dz", "bt" );
  addExtraCountry( "el", "gr" );
  addExtraCountry( "en", "gb" );
  addExtraCountry( "es", "es" );
  addExtraCountry( "et", "ee" );
  addExtraCountry( "eu", "es" );
  addExtraCountry( "fa", "ir" );
  addExtraCountry( "fi", "fi" );
  addExtraCountry( "fo", "fo" );
  addExtraCountry( "fr", "fr" );
  addExtraCountry( "fy", "nl" );
  addExtraCountry( "ga", "ie" );
  addExtraCountry( "gd", "gb" );
  addExtraCountry( "gl", "es" );
  addExtraCountry( "gu", "in" );
  addExtraCountry( "gv", "gb" );
  addExtraCountry( "ha", "ng" );
  addExtraCountry( "he", "il" );
  addExtraCountry( "hi", "in" );
  addExtraCountry( "hr", "hr" );
  addExtraCountry( "ht", "ht" );
  addExtraCountry( "hu", "hu" );
  addExtraCountry( "hy", "am" );
  addExtraCountry( "id", "id" );
  addExtraCountry( "ig", "ng" );
  addExtraCountry( "ik", "ca" );
  addExtraCountry( "is", "is" );
  addExtraCountry( "it", "it" );
  addExtraCountry( "iu", "ca" );
  addExtraCountry( "iw", "il" );
  addExtraCountry( "ja", "jp" );
  addExtraCountry( "jb", "jb" );
  addExtraCountry( "ka", "ge" );
  addExtraCountry( "kk", "kz" );
  addExtraCountry( "kl", "gl" );
  addExtraCountry( "km", "kh" );
  addExtraCountry( "kn", "in" );
  addExtraCountry( "ko", "kr" );
  addExtraCountry( "ku", "tr" );
  addExtraCountry( "kw", "gb" );
  addExtraCountry( "ky", "kg" );
  addExtraCountry( "lg", "ug" );
  addExtraCountry( "li", "be" );
  addExtraCountry( "lo", "la" );
  addExtraCountry( "lt", "lt" );
  addExtraCountry( "lv", "lv" );
  addExtraCountry( "mg", "mg" );
  addExtraCountry( "mi", "nz" );
  addExtraCountry( "mk", "mk" );
  addExtraCountry( "ml", "in" );
  addExtraCountry( "mn", "mn" );
  addExtraCountry( "mr", "in" );
  addExtraCountry( "ms", "my" );
  addExtraCountry( "mt", "mt" );
  addExtraCountry( "nb", "no" );
  addExtraCountry( "ne", "np" );
  addExtraCountry( "nl", "nl" );
  addExtraCountry( "nn", "no" );
  addExtraCountry( "nr", "za" );
  addExtraCountry( "oc", "fr" );
  addExtraCountry( "om", "et" );
  addExtraCountry( "or", "in" );
  addExtraCountry( "pa", "pk" );
  addExtraCountry( "pl", "pl" );
  addExtraCountry( "pt", "pt" );
  addExtraCountry( "ro", "ro" );
  addExtraCountry( "ru", "ru" );
  addExtraCountry( "rw", "rw" );
  addExtraCountry( "sa", "in" );
  addExtraCountry( "sc", "it" );
  addExtraCountry( "sd", "in" );
  addExtraCountry( "se", "no" );
  addExtraCountry( "si", "lk" );
  addExtraCountry( "sk", "sk" );
  addExtraCountry( "sl", "si" );
  addExtraCountry( "so", "so" );
  addExtraCountry( "sq", "al" );
  addExtraCountry( "sr", "rs" );
  addExtraCountry( "ss", "za" );
  addExtraCountry( "st", "za" );
  addExtraCountry( "sv", "se" );
  addExtraCountry( "ta", "in" );
  addExtraCountry( "te", "in" );
  addExtraCountry( "tg", "tj" );
  addExtraCountry( "th", "th" );
  addExtraCountry( "ti", "er" );
  addExtraCountry( "tk", "tm" );
  addExtraCountry( "tl", "ph" );
  addExtraCountry( "tn", "za" );
  addExtraCountry( "tr", "tr" );
  addExtraCountry( "ts", "za" );
  addExtraCountry( "tt", "ru" );
  addExtraCountry( "ug", "cn" );
  addExtraCountry( "uk", "ua" );
  addExtraCountry( "ur", "pk" );
  addExtraCountry( "uz", "uz" );
  addExtraCountry( "ve", "za" );
  addExtraCountry( "vi", "vn" );
  addExtraCountry( "wa", "be" );
  addExtraCountry( "wo", "sn" );
  addExtraCountry( "xh", "za" );
  addExtraCountry( "yi", "us" );
  addExtraCountry( "yo", "ng" );
  addExtraCountry( "zh", "cn" );
  addExtraCountry( "zu", "za" );
}

} // namespace

/// babylon languages
#ifndef blgCode2Int
  #define blgCode2Int( index, code0, code1 ) \
    ( ( (uint32_t)index ) << 16 ) + ( ( (uint32_t)code1 ) << 8 ) + (uint32_t)code0
#endif
const BabylonLang BabylonDb[] = {
  { blgCode2Int( 1, 'z', 'h' ), "tw", "Traditional Chinese", QT_TR_NOOP( "Traditional Chinese" ) },
  { blgCode2Int( 2, 'z', 'h' ), "cn", "Simplified Chinese", QT_TR_NOOP( "Simplified Chinese" ) },
  { blgCode2Int( 3, 0, 0 ), "other", "Other", QT_TR_NOOP( "Other" ) },
  { blgCode2Int( 4, 'z', 'h' ),
    "cn",
    "Other Simplified Chinese dialects",
    QT_TR_NOOP( "Other Simplified Chinese dialects" ) },
  { blgCode2Int( 5, 'z', 'h' ),
    "tw",
    "Other Traditional Chinese dialects",
    QT_TR_NOOP( "Other Traditional Chinese dialects" ) },
  { blgCode2Int( 6, 0, 0 ),
    "other",
    "Other Eastern-European languages",
    QT_TR_NOOP( "Other Eastern-European languages" ) },
  { blgCode2Int( 7, 0, 0 ),
    "other",
    "Other Western-European languages",
    QT_TR_NOOP( "Other Western-European languages" ) },
  { blgCode2Int( 8, 'r', 'u' ), "ru", "Other Russian languages", QT_TR_NOOP( "Other Russian languages" ) },
  { blgCode2Int( 9, 'j', 'a' ), "jp", "Other Japanese languages", QT_TR_NOOP( "Other Japanese languages" ) },
  { blgCode2Int( 10, 0, 0 ), "other", "Other Baltic languages", QT_TR_NOOP( "Other Baltic languages" ) },
  { blgCode2Int( 11, 'e', 'l' ), "gr", "Other Greek languages", QT_TR_NOOP( "Other Greek languages" ) },
  { blgCode2Int( 12, 'k', 'o' ), "kr", "Other Korean dialects", QT_TR_NOOP( "Other Korean dialects" ) },
  { blgCode2Int( 13, 't', 'r' ), "tr", "Other Turkish dialects", QT_TR_NOOP( "Other Turkish dialects" ) },
  { blgCode2Int( 14, 't', 'h' ), "th", "Other Thai dialects", QT_TR_NOOP( "Other Thai dialects" ) },
  { blgCode2Int( 15, 0, 0 ), "dz", "Tamazight", QT_TR_NOOP( "Tamazight" ) } };

BabylonLang getBabylonLangByIndex( int index )
{
  return BabylonDb[ index ];
}

quint32 findBlgLangIDByEnglishName( gd::wstring const & lang )
{
  QString enName = QString::fromStdU32String( lang );
  for ( const auto & idx : BabylonDb ) {
    if ( QString::compare( idx.englishName, enName, Qt::CaseInsensitive ) == 0 )
      return idx.id;
  }
  return 0;
}

QString englishNameForId( Id id )
{
  if ( id >= 0x010000 && id <= 0x0fffff ) //babylon
  {
    return BabylonDb[ ( ( id >> 16 ) & 0x0f ) - 1 ].englishName;
  }
  const auto i = Db::instance().getIso2ToLangData().find( LangCoder::intToCode2( id ) );

  if ( i == Db::instance().getIso2ToLangData().end() )
    return {};

  return i->english;
}

QString localizedNameForId( Id id )
{
  if ( id >= 0x010000 && id <= 0x0fffff ) //babylon
  {
    return QCoreApplication::translate( "Language", BabylonDb[ ( ( id >> 16 ) & 0x0f ) - 1 ].localizedName );
  }
  const auto i = Db::instance().getIso2ToLangData().find( LangCoder::intToCode2( id ) );

  if ( i == Db::instance().getIso2ToLangData().end() )
    return {};

  return i->localized;
}

QString countryCodeForId( Id id )
{
  if ( id >= 0x010000 && id <= 0x0fffff ) //babylon
  {
    return BabylonDb[ ( ( id >> 16 ) & 0x0f ) - 1 ].contryCode;
  }
  const auto i = Db::instance().getIso2ToLangData().find( LangCoder::intToCode2( id ) );

  if ( i == Db::instance().getIso2ToLangData().end() )
    return {};

  return i->country;
}

QString localizedStringForId( Id langId )
{
  QString name = localizedNameForId( langId );

  if ( name.isEmpty() )
    return name;

  QString iconId = countryCodeForId( langId );

  if ( iconId.isEmpty() )
    return name;
  return QString( "<img src=\":/flags/%1.png\"> %2" ).arg( iconId, name );
}

QString languageForLocale( const QString & locale )
{
  return QCoreApplication::translate( "Language::Db",
                                      Db::instance().locale2LanguageMap()[ locale ].toStdString().c_str() );
}
} // namespace Language
