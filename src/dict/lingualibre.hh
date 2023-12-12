#ifndef GOLDENDICT_LINGUALIBRE_H
#define GOLDENDICT_LINGUALIBRE_H

#include "dictionary.hh"
#include "config.hh"
#include <QNetworkAccessManager>

namespace Lingua {

std::vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, Config::Lingua const &, QNetworkAccessManager & );

} // namespace Lingua

#endif //GOLDENDICT_LINGUALIBRE_H
