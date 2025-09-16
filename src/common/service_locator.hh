#pragma once

#include "article_netmgr.hh"
#include "audio/audioplayerinterface.hh"
#include "instances.hh"
#include "config.hh"
#include "dict/dictionary.hh"
#include <vector>
#include <memory>

class ServiceLocator
{
public:
  static ServiceLocator & instance()
  {
    static ServiceLocator instance;
    return instance;
  }

  // Set global services
  void setNetworkManager( ArticleNetworkAccessManager * nm )
  {
    networkManager = nm;
  }
  void setAudioPlayer( AudioPlayerInterface * player )
  {
    audioPlayer.reset( player );
  }
  void setDictionaries( const std::vector< sptr< Dictionary::Class > > & dicts )
  {
    allDictionaries = &dicts;
  }
  void setGroups( const Instances::Groups & grps )
  {
    groups = &grps;
  }
  void setConfig( const Config::Class & cfg )
  {
    config = &cfg;
  }

  // Get services
  ArticleNetworkAccessManager & getNetworkManager()
  {
    return *networkManager;
  }
  const AudioPlayerPtr & getAudioPlayer()
  {
    return audioPlayer;
  }
  const std::vector< sptr< Dictionary::Class > > & getAllDictionaries()
  {
    return *allDictionaries;
  }
  const Instances::Groups & getGroups()
  {
    return *groups;
  }
  const Config::Class & getConfig()
  {
    return *config;
  }

private:
  ServiceLocator()                                     = default;
  ~ServiceLocator()                                    = default;
  ServiceLocator( const ServiceLocator & )             = delete;
  ServiceLocator & operator=( const ServiceLocator & ) = delete;

  ArticleNetworkAccessManager * networkManager = nullptr;
  AudioPlayerPtr audioPlayer;
  const std::vector< sptr< Dictionary::Class > > * allDictionaries = nullptr;
  const Instances::Groups * groups                                 = nullptr;
  const Config::Class * config                                     = nullptr;
};
