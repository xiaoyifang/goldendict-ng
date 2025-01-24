/* This file is (c) 2013 Timon Wong <timon86.wang@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#ifdef TTS_SUPPORT

  #include "voiceengines.hh"
  #include "audiolink.hh"
  #include "htmlescape.hh"
  #include "text.hh"

  #include <string>
  #include <map>

  #include <QDir>
  #include <QFileInfo>
  #include <QCryptographicHash>

  #include "utils.hh"

namespace VoiceEngines {

using namespace Dictionary;
using std::string;
using std::u32string;
using std::map;

inline string toMd5( QByteArray const & b )
{
  return string( QCryptographicHash::hash( b, QCryptographicHash::Md5 ).toHex().constData() );
}

class VoiceEnginesDictionary: public Dictionary::Class
{
private:

  Config::VoiceEngine voiceEngine;

public:

  VoiceEnginesDictionary( Config::VoiceEngine const & voiceEngine ):
    Dictionary::Class( toMd5( voiceEngine.name.toUtf8() ), vector< string >() ),
    voiceEngine( voiceEngine )
  {
  }

  string getName() noexcept override
  {
    return voiceEngine.name.toUtf8().data();
  }


  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( u32string const & word, unsigned long maxResults ) override;

  sptr< DataRequest >
  getArticle( u32string const &, vector< u32string > const & alts, u32string const &, bool ) override;

protected:

  void loadIcon() noexcept override;
};

sptr< WordSearchRequest > VoiceEnginesDictionary::prefixMatch( u32string const & /*word*/,
                                                               unsigned long /*maxResults*/ )

{
  WordSearchRequestInstant * sr = new WordSearchRequestInstant();
  sr->setUncertain( true );
  return std::shared_ptr< WordSearchRequestInstant >( sr );
}

sptr< Dictionary::DataRequest >
VoiceEnginesDictionary::getArticle( u32string const & word, vector< u32string > const &, u32string const &, bool )

{
  string result;
  string wordUtf8( Text::toUtf8( word ) );

  result += "<div class=\"audio-play\"><div class=\"audio-play-item\">";

  QUrl url;
  url.setScheme( "gdtts" );
  url.setHost( "localhost" );
  url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( wordUtf8.c_str() ) ) );
  QList< std::pair< QString, QString > > query;
  query.push_back( std::pair< QString, QString >( "engine", QString::fromStdString( getId() ) ) );
  Utils::Url::setQueryItems( url, query );

  string encodedUrl = url.toEncoded().data();
  string ref        = string( "\"" ) + encodedUrl + "\"";
  addAudioLink( encodedUrl, getId() );

  result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
  result += "<a href=" + ref + ">" + Html::escape( wordUtf8 ) + "</a>";
  result += "</div></div>";

  auto ret = std::make_shared< DataRequestInstant >( true );
  ret->appendString( result );
  return ret;
}

void VoiceEnginesDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  if ( !voiceEngine.iconFilename.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), voiceEngine.iconFilename );
    if ( fInfo.isFile() )
      loadIconFromFile( fInfo.absoluteFilePath(), true );
  }
  if ( dictionaryIcon.isNull() )
    dictionaryIcon = QIcon( ":/icons/text2speech.svg" );
  dictionaryIconLoaded = true;
}

vector< sptr< Dictionary::Class > > makeDictionaries( Config::VoiceEngines const & voiceEngines )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & voiceEngine : voiceEngines ) {
    if ( voiceEngine.enabled )
      result.push_back( std::make_shared< VoiceEnginesDictionary >( voiceEngine ) );
  }

  return result;
}

} // namespace VoiceEngines

#endif
