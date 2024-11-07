#pragma once

#include "dict/dictionary.hh"
#include "config.hh"

/// Support for servers supporting DICT protocol.
namespace DictServer {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( Config::DictServers const & servers );

} // namespace DictServer

