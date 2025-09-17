/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "audiolink.hh"
#include "globalbroadcaster.hh"

std::string addAudioLink( const std::string & url, const std::string & dictionaryId )
{
  return addAudioLink( QString::fromStdString( url ), dictionaryId );
}

std::string addAudioLink( const QString & url, const std::string & dictionaryId )
{
  if ( url.isEmpty() ) {
    return {};
  }
  GlobalBroadcaster::instance()->pronounce_engine.sendAudio( dictionaryId, url );
  return "";
}
