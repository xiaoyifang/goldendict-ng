#ifndef __EPWING_HH__INCLUDED__
#define __EPWING_HH__INCLUDED__

#include "dict/dictionary.hh"
#include "epwing_book.hh"
#include "btreeidx.hh"
#include "chunkedstorage.hh"
/// Support for the Epwing dictionaries.
namespace Epwing {

using std::vector;
using std::string;

void addWordToChunks( Epwing::Book::EpwingHeadword & head,
                      ChunkedStorage::Writer & chunks,
                      BtreeIndexing::IndexedWords & indexedWords,
                      int & wordCount,
                      int & articleCount );

vector< sptr< Dictionary::Class > >
makeDictionaries( vector< string > const & fileNames, string const & indicesDir, Dictionary::Initializing & );
} // namespace Epwing

#endif // __EPWING_HH__INCLUDED__
