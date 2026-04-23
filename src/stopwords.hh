#pragma once

#include <string>
#include <vector>

namespace Xapian {
class SimpleStopper;
}

namespace Stopwords {

std::vector< std::string > getStopwords();
Xapian::SimpleStopper * getStopper();

} // namespace Stopwords
