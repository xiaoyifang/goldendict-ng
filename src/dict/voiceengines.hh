/* This file is (c) 2013 Timon Wong <timon86.wang@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#pragma once
#ifdef TTS_SUPPORT

  #include "dictionary.hh"
  #include "config.hh"
  #include "text.hh"
  #include <QCryptographicHash>

namespace VoiceEngines {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( const Config::VoiceEngines & voiceEngines );

} // namespace VoiceEngines

#endif
