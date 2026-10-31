/* Headword cache implementation.
 * See headwordscache.hh for file format documentation.
 */

#include "headwordscache.hh"
#include "dict/btreeidx.hh"
#include "common/utils.hh"

#include <QFile>
#include <QBuffer>
#include <QByteArray>
#include <QtConcurrentRun>
#include <limits>
#include <QThreadPool>
#include <QFutureSynchronizer>
#include <QMutexLocker>
#include <QCryptographicHash>
#include <algorithm>

namespace HeadwordCache {

// ================================================================
// HeadwordCache
// ================================================================

HeadwordCache::HeadwordCache() = default;

HeadwordCache::~HeadwordCache()
{
  if ( mapped ) {
    file.unmap( mapped );
    mapped = nullptr;
  }
  if ( file.isOpen() ) {
    file.close();
  }
}

bool HeadwordCache::open( const std::string & path )
{
  // Reset any previous state first -- allows re-opening on the same object.
  if ( mapped ) {
    file.unmap( mapped );
    mapped = nullptr;
  }
  if ( file.isOpen() ) {
    file.close();
  }
  valid = false;

  cachePath = QString::fromStdString( path );

  file.setFileName( cachePath );
  if ( !file.open( QIODevice::ReadOnly ) ) {
    valid = false;
    return false;
  }

  fileSize = file.size();
  if ( fileSize < HEADER_SIZE ) {
    file.close();
    valid = false;
    return false;
  }

  mapped = file.map( 0, fileSize );
  if ( !mapped ) {
    file.close();
    valid = false;
    return false;
  }

  // Validate header
  auto * hdr = reinterpret_cast< const FileHeader * >( mapped );

  if ( std::memcmp( hdr->magic, MAGIC, 8 ) != 0 ) {
    file.unmap( mapped );
    mapped = nullptr;
    file.close();
    valid = false;
    return false;
  }

  if ( hdr->version != FILE_VERSION ) {
    file.unmap( mapped );
    mapped = nullptr;
    file.close();
    valid = false;
    return false;
  }

  header = *hdr;

  // Sanity check: index_table_offset must be >= HEADER_SIZE,
  // and the index table must fit inside the file. Compute in signed
  // arithmetic to avoid sign-compare warnings.
  const qint64 tableEnd = static_cast< qint64 >( header.index_table_offset )
    + static_cast< qint64 >( header.headword_count ) * static_cast< qint64 >( sizeof( uint32_t ) );
  if ( header.index_table_offset < HEADER_SIZE || tableEnd > fileSize ) {
    file.unmap( mapped );
    mapped = nullptr;
    file.close();
    valid = false;
    return false;
  }

  valid = true;
  return true;
}

QString HeadwordCache::at( uint32_t row ) const
{
  if ( !valid || row >= header.headword_count ) {
    return {};
  }

  // Read offset from index table. Offsets are relative to the START OF THE
  // DATA AREA (i.e. byte HEADER_SIZE in the file), not absolute.
  const uint32_t * indexTable = reinterpret_cast< const uint32_t * >( mapped + header.index_table_offset );
  const uint32_t offset       = indexTable[ row ];
  const uint32_t absOffset    = HEADER_SIZE + offset;

  // Sanity: absolute offset must land inside the data area
  if ( absOffset >= header.index_table_offset ) {
    return {};
  }

  // Find null terminator (bounded by the end of the data area)
  const char * start = reinterpret_cast< const char * >( mapped + absOffset );
  const char * end   = reinterpret_cast< const char * >( mapped + header.index_table_offset );
  const char * p     = std::find( start, end, '\0' );

  return QString::fromUtf8( start, static_cast< int >( p - start ) );
}

bool HeadwordCache::validateHeader( const std::string & path )
{
  QFile f( QString::fromStdString( path ) );
  if ( !f.open( QIODevice::ReadOnly ) ) {
    return false;
  }
  if ( f.size() < HEADER_SIZE ) {
    f.close();
    return false;
  }
  FileHeader hdr{};
  if ( f.read( reinterpret_cast< char * >( &hdr ), HEADER_SIZE ) != HEADER_SIZE ) {
    f.close();
    return false;
  }
  f.close();

  if ( std::memcmp( hdr.magic, MAGIC, 8 ) != 0 ) {
    return false;
  }
  if ( hdr.version != FILE_VERSION ) {
    return false;
  }

  qint64 idxEnd = hdr.index_table_offset + hdr.headword_count * sizeof( uint32_t );
  return idxEnd <= f.size();
}

// ================================================================
// HeadwordCacheBuilder
// ================================================================

bool HeadwordCacheBuilder::writeAndCommit( const std::string & finalPath, const QStringList & sortedHeadwords )
{
  std::string tmpPath = finalPath + "_tmp";

  QFile tmpFile( QString::fromStdString( tmpPath ) );
  if ( !tmpFile.open( QIODevice::WriteOnly ) ) {
    qWarning() << "HeadwordCacheBuilder: cannot open" << QString::fromStdString( tmpPath );
    return false;
  }

  const uint32_t count = static_cast< uint32_t >( sortedHeadwords.size() );

  // Phase 1: write data area (null-terminated UTF-8), collecting offsets
  QByteArray dataArea;
  dataArea.reserve( count * 32 ); // rough estimate
  QList< uint32_t > offsets;
  offsets.reserve( count );

  for ( const auto & hw : sortedHeadwords ) {
    offsets.append( static_cast< uint32_t >( dataArea.size() ) );
    dataArea += hw.toUtf8();
    dataArea += '\0';
  }

  const uint32_t indexTableOffset = HEADER_SIZE + static_cast< uint32_t >( dataArea.size() );

  // Phase 2: write header + data area + index table
  FileHeader hdr{};
  std::memcpy( hdr.magic, MAGIC, 8 );
  hdr.version            = FILE_VERSION;
  hdr.headword_count     = count;
  hdr.index_table_offset = indexTableOffset;

  if ( tmpFile.write( reinterpret_cast< const char * >( &hdr ), HEADER_SIZE ) != HEADER_SIZE ) {
    tmpFile.remove();
    return false;
  }
  if ( !tmpFile.write( dataArea ) ) {
    tmpFile.remove();
    return false;
  }
  if ( !tmpFile.write( reinterpret_cast< const char * >( offsets.constData() ),
                       static_cast< qint64 >( offsets.size() ) * sizeof( uint32_t ) ) ) {
    tmpFile.remove();
    return false;
  }

  tmpFile.close();

  // Validate before commit
  if ( !HeadwordCache::validateHeader( tmpPath ) ) {
    qWarning() << "HeadwordCacheBuilder: tmp file failed validation";
    QFile::remove( QString::fromStdString( tmpPath ) );
    return false;
  }

  // Atomic commit: remove old file, rename tmp -> final
  QString finalQPath = QString::fromStdString( finalPath );
  QFile::remove( finalQPath );

  if ( !QFile::rename( QString::fromStdString( tmpPath ), finalQPath ) ) {
    // Cross-drive or other failure -- try copy
    if ( !QFile::copy( QString::fromStdString( tmpPath ), finalQPath ) ) {
      qWarning() << "HeadwordCacheBuilder: cannot commit" << finalQPath;
      QFile::remove( QString::fromStdString( tmpPath ) );
      return false;
    }
    QFile::remove( QString::fromStdString( tmpPath ) );
  }

  return true;
}

bool HeadwordCacheBuilder::finalizeFromTmp( const std::string & finalPath )
{
  const std::string dataTmpPath  = finalPath + "_data.tmp";
  const std::string progressPath = finalPath + ".progress";

  // Read collected headwords from the scratch file
  QFile allData( QString::fromStdString( dataTmpPath ) );
  if ( !allData.open( QIODevice::ReadOnly ) ) {
    qWarning() << "HeadwordCacheBuilder: cannot reopen data tmp for sort";
    return false;
  }
  const QByteArray rawData = allData.readAll();
  allData.close();

  QStringList allHeadwords;
  int start    = 0;
  const int sz = rawData.size();
  for ( int i = 0; i < sz; ++i ) {
    if ( rawData.at( i ) == '\0' ) {
      if ( i > start ) { // skip empty strings (defensive: stray \0\0 pairs)
        allHeadwords.append( QString::fromUtf8( rawData.constData() + start, i - start ) );
      }
      start = i + 1;
    }
  }

  if ( allHeadwords.isEmpty() ) {
    qWarning() << "HeadwordCacheBuilder: no headwords in data tmp";
    QFile::remove( QString::fromStdString( dataTmpPath ) );
    QFile::remove( QString::fromStdString( progressPath ) );
    return false;
  }

  // Sort alphabetically (case-insensitive)
  std::sort( allHeadwords.begin(), allHeadwords.end(), []( const QString & a, const QString & b ) {
    return QString::compare( a, b, Qt::CaseInsensitive ) < 0;
  } );

  // Deduplicate exact matches only (case-sensitive): dictionaries legitimately
  // contain case-distinct headwords ("CAD"/"cad"). After the case-insensitive
  // sort above, exact duplicates are guaranteed to be adjacent.
  const auto newEnd =
    std::unique( allHeadwords.begin(), allHeadwords.end(), []( const QString & a, const QString & b ) {
      return a == b;
    } );
  allHeadwords.erase( newEnd, allHeadwords.end() );

  // Build the final .hwcache file atomically
  if ( !writeAndCommit( finalPath, allHeadwords ) ) {
    qWarning() << "HeadwordCacheBuilder: commit failed";
    return false;
  }

  // Clean up temporary files
  QFile::remove( QString::fromStdString( dataTmpPath ) );
  QFile::remove( QString::fromStdString( progressPath ) );
  allHeadwords.clear();

  return true;
}

// ================================================================
// HeadwordIndexing (background manager)
// ================================================================

class IndexingRunnable: public QObject, public QRunnable
{
  Q_OBJECT

  QAtomicInt & isCancelled;
  const std::vector< sptr< Dictionary::Class > > & dictionaries;
  QSemaphore & hasExited;

public:
  IndexingRunnable( QAtomicInt & cancelled,
                    const std::vector< sptr< Dictionary::Class > > & dicts,
                    QSemaphore & hasExited_ ):
    isCancelled( cancelled ),
    dictionaries( dicts ),
    hasExited( hasExited_ )
  {
    setAutoDelete( true );
  }

  ~IndexingRunnable() override
  {
    hasExited.release();
  }

  void run() override
  {
    try {
      for ( const auto & dictionary : dictionaries ) {
        if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
          break;
        }
        if ( dictionary->supportsHeadwordsCache() && !dictionary->haveHeadwordsCache() ) {
          const QString name = QString::fromUtf8( dictionary->getName().c_str() );
          emit sendNowIndexingName( name );
          dictionary->makeHeadwordsCache( isCancelled );
        }
      }
    }
    catch ( std::exception & ex ) {
      qWarning( "HeadwordIndexing: exception: %s", ex.what() );
    }
    emit indexingFinished();
  }

signals:
  void sendNowIndexingName( QString );
  void indexingFinished();
};

HeadwordIndexing::HeadwordIndexing( const std::vector< sptr< Dictionary::Class > > & dicts ):
  dictionaries( dicts ),
  started( false )
{
  timer.setInterval( 2000 );
  connect( &timer, &QTimer::timeout, this, &HeadwordIndexing::onTimeout );
}

HeadwordIndexing::~HeadwordIndexing()
{
  timer.stop();
  stopIndexing();
}

void HeadwordIndexing::setDictionaries( const std::vector< sptr< Dictionary::Class > > & dicts )
{
  stopIndexing();
  dictionaries = dicts;
}

void HeadwordIndexing::doIndexing()
{
  if ( started ) {
    return;
  }
  while ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    isCancelled.deref();
  }

  auto * r = new IndexingRunnable( isCancelled, dictionaries, indexingExited );
  runnable = r;
  QObject::connect( r, &IndexingRunnable::sendNowIndexingName, this, &HeadwordIndexing::setNowIndexedName );
  QObject::connect( r, &IndexingRunnable::indexingFinished, this, &HeadwordIndexing::onIndexingFinished );

  QThreadPool::globalInstance()->start( r );
  timer.start();
  started = true;
}

void HeadwordIndexing::stopIndexing()
{
  if ( started ) {
    if ( !Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      isCancelled.ref();
    }
    indexingExited.acquire();
    timer.stop();
    started = false;
    setNowIndexedName( QString() );
  }
}

void HeadwordIndexing::onTimeout()
{
  if ( runnable.isNull() ) {
    return;
  }

  QString indexingNames;
  for ( const auto & dictionary : dictionaries ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      break;
    }
    if ( dictionary->haveHeadwordsCache() ) {
      continue;
    }
    if ( !dictionary->supportsHeadwordsCache() ) {
      continue;
    }
    auto progress = dictionary->getHeadwordsCacheProgress();
    if ( progress > 0 && progress < 100 ) {
      if ( !indexingNames.isEmpty() ) {
        indexingNames.append( "," );
      }
      indexingNames.append(
        QString( "%1......%%2" ).arg( QString::fromStdString( dictionary->getName() ) ).arg( progress ) );
    }
  }
  if ( !indexingNames.isEmpty() ) {
    setNowIndexedName( indexingNames );
  }
}

void HeadwordIndexing::onIndexingFinished()
{
  if ( started ) {
    timer.stop();
    started = false;
  }
  setNowIndexedName( QString() );
  emit indexingFinished();
}

void HeadwordIndexing::setNowIndexedName( const QString & name )
{
  {
    QMutexLocker _( &nameMutex );
    nowIndexing = name;
  }
  emit newIndexingName( name );
}

QString HeadwordIndexing::nowIndexingName()
{
  QMutexLocker _( &nameMutex );
  return nowIndexing;
}

} // namespace HeadwordCache

#include "headwordscache.moc"
