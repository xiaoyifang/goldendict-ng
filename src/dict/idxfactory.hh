#include "baseindex.hh"  // Assume the header file for BaseIndex is baseindex.hh
#include "btreeindex.hh" // Assume the header file for BtreeIndex is btreeindex.hh
#include "xapianindex.hh" // Assume the header file for XapianIndex is xapianindex.hh

// Define the IndexType enumeration
enum class IndexType {
  Btree,
  Xapian
};

class Idxfactory {
public:
  static BaseIndex* createIndex(IndexType type) {
    switch(type) {
      case IndexType::Btree: return new BtreeIndex();
      case IndexType::Xapian: return new XapianIndex();
      default: return new BtreeIndex();
    }
  }
};