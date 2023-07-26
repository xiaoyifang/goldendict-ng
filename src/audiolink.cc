/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "audiolink.hh"
#include "globalbroadcaster.hh"

std::string addAudioLink( std::string const & url, std::string const & dictionaryId )
{
  return addAudioLink( QString::fromStdString( url ), dictionaryId );
}

std::string addAudioLink( QString const & url, std::string const & dictionaryId )
{
  if ( url.isEmpty() || url.length() < 2 )
    return {};
  GlobalBroadcaster::instance()->pronounce_engine.sendAudio(
    dictionaryId, url.mid( 1, url.length() - 2 ) );

  return std::string( "<script type=\"text/javascript\">" + makeAudioLinkScript( url.toStdString(), dictionaryId ) + "</script>" );
}

std::string makeAudioLinkScript( std::string const & url, std::string const & dictionaryId )
{
  /// Convert "'" to "\'" - this char broke autoplay of audiolinks

  std::string ref;
  bool escaped = false;
  for ( const char ch : url ) {
    if ( escaped ) {
      ref += ch;
      escaped = false;
      continue;
    }
    if ( ch == '\'' )
      ref += '\\';
    ref += ch;
    escaped = ( ch == '\\' );
  }

  const std::string audioLinkForDict = QString::fromStdString( R"(
if(!gdAudioMap.has('%1')){
    gdAudioMap.set('%1',%2);
}
)" )
                                         .arg( QString::fromStdString( dictionaryId ), QString::fromStdString( url ) )
                                         .toStdString();
  return "gdAudioLinks.first = gdAudioLinks.first || " + ref + ";" + audioLinkForDict;
}
