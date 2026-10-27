/* Headword cache: a sorted, mmap'd binary file for fast headword access.
 * Replaces the expensive btree leaf traversal used by HeadwordListModel.
 * Built in the background on first open of the headword dialog.
 *
 * File format (little-endian, all numeric fields):
 *   [0..7]   magic       = "GDBTHDW0"   (8 bytes)
 *   [8..11]  version     = 1            (uint32)
 *   [12..15] headword_count            (uint32)
 *   [16..19] index_table_offset        (uint32) -- byte offset to index table
 *   [20 .. index_table_offset-1]       -- data area: null-terminated UTF-8 strings
 *   [index_table_offset .. file_size)  -- index table: uint32_t[headword_count]
 *                                         each value = byte offset of that headword in the data area
 *
 * The index table is always sorted alphabetically (by the UTF-8 byte order,
 * which matches QString's case-sort when the strings are folded consistently).
 */

#pragma once

#include <QByteArray>
#include <QFile>
#include <QSemaphore>
#include <QTimer>
#include <QPointer>
#include <QRunnable>
#include <QObject>
#include <vector>
#include "dict/dictionary.hh"

namespace HeadwordCache {

// -----------------------------
// File format constants
// -----------------------------
constexpr const char MAGIC[ 8 ]  = { 'G', 'D', 'B', 'T', 'H', 'D', 'W', '0' };
constexpr uint32_t FILE_VERSION  = 1;
constexpr uint32_t HEADER_SIZE   = 20; // magic + version + count + index_table_offset

#pragma pack( push, 1 )
struct FileHeader
{
  char     magic[ 8 ];
  uint32_t version;
  uint32_t headword_count;
  uint32_t index_table_offset; // offset to the uint32_t array
};
#pragma pack( pop )

// -----------------------------
// HeadwordCache -- read-only accessor over the mmap'd file
// -----------------------------
class HeadwordCache
{
public:
  HeadwordCache();
  ~HeadwordCache();

  /// Opens and memory-maps the cache file. Returns true on success.
  /// On failure (missing, corrupt, wrong version) returns false.
  bool open( const std::string & path );

  /// True when the cache is open and valid
  bool isValid() const
  {
    return valid;
  }

  /// Number of headwords in the cache
  uint32_t count() const
  {
    return valid ? header.headword_count : 0;
  }

  /// Access the headword at the given row index.
  /// Row index is 0-based and corresponds to alphabetical order.
  /// Returns an empty string on out-of-range or invalid cache.
  QString at( uint32_t row ) const;

  /// Full iteration support -- useful for export.
  /// Calls visitor( QString ) for each headword in order.
  template < typename Fn >
  void forEach( Fn && visitor ) const
  {
    if ( !valid )
      return;
    for ( uint32_t i = 0; i < header.headword_count; ++i ) {
      visitor( at( i ) );
    }
  }

  /// Validates the file header. Used by the builder before renaming .tmp to final.
  static bool validateHeader( const std::string & path );

private:
  bool valid = false;
  QString cachePath;
  QFile   file;
  uchar * mapped  = nullptr;
  qint64  fileSize = 0;
  FileHeader header{};
};

// -----------------------------
// Builder -- reads collected headwords from a _data.tmp scratch file,
// sorts them, and writes the final binary .hwcache format atomically.
// The batch traversal (which walks btree leaves incrementally with resume
// support) lives in BtreeDictionary::makeHeadwordsCache where it can
// access protected BtreeIndex methods.
// -----------------------------
class HeadwordCacheBuilder
{
public:
  /// Reads null-terminated UTF-8 strings from <path>_data.tmp, sorts them,
  /// and writes the final sorted binary file to <path>.
  /// Returns true on success. Caller is responsible for creating the _data.tmp
  /// file (via batch btree traversal in BtreeDictionary::makeHeadwordsCache).
  static bool finalizeFromTmp( const std::string & path );

  /// Validates that <path> is a well-formed .hwcache file.
  static bool validateHeader( const std::string & path );

private:
  /// Writes the final sorted binary format atomically (tmp + rename).
  static bool writeAndCommit( const std::string & finalPath, const QStringList & sortedHeadwords );
};

// -----------------------------
// Background indexing manager (mirrors FTS::FtsIndexing)
// -----------------------------
class IndexingRunnable; // forward

class HeadwordIndexing : public QObject
{
  Q_OBJECT

public:
  explicit HeadwordIndexing( const std::vector< sptr< Dictionary::Class > > & dicts );
  virtual ~HeadwordIndexing();

  void setDictionaries( const std::vector< sptr< Dictionary::Class > > & dicts );

  /// Start headword cache indexing in the background. Idempotent if already
  /// running (call stopIndexing() first if you need to force a restart).
  void doIndexing();

  /// Cancel and wait for the indexing thread to exit.
  void stopIndexing();

  QString nowIndexingName();

private:
  QAtomicInt isCancelled;
  QSemaphore indexingExited;
  std::vector< sptr< Dictionary::Class > > dictionaries;
  bool started = false;
  QString nowIndexing;
  QMutex nameMutex;
  QTimer timer;
  QPointer< IndexingRunnable > runnable;

private slots:
  void setNowIndexedName( const QString & name );
  void onTimeout();
  void onIndexingFinished();

signals:
  void newIndexingName( QString name );

  /// Emitted when all dictionaries have been processed (or cancelled).
  /// MainWindow can connect this to refresh any open headword dialogs.
  void indexingFinished();
};

} // namespace HeadwordCache
