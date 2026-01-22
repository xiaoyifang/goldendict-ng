#include "headwordsmodel.hh"
#include "btreeidx.hh"
#include "utils.hh"
#include "text.hh"
#include <QtConcurrent>

HeadwordListModel::HeadwordListModel( QObject * parent ):
  QAbstractListModel( parent ),
  filtering( false ),
  totalSize( 0 ),
  maxFilterResults( 1000 ),
  finished( false ),
  _dict( nullptr ),
  index( 0 ),
  ptr( nullptr ),
  currentOffset( 0 ),
  useIndex( false ),
  indexBuilding( false )
{
}

int HeadwordListModel::rowCount( const QModelIndex & parent ) const
{
  return parent.isValid() ? 0 : words.size();
}

int HeadwordListModel::totalCount() const
{
  return totalSize;
}

bool HeadwordListModel::isFinish() const
{
  return finished || ( words.size() >= totalSize );
}

// export headword
QString HeadwordListModel::getRow( int row )
{
  if ( fileSortedList.empty() ) {
    fileSortedList << words;
    fileSortedList.sort();
  }
  return fileSortedList.at( row );
}

void HeadwordListModel::setFilter( const QRegularExpression & reg )
{
  // If headword index is available, use it for searching
  if ( useIndex && !reg.pattern().isEmpty() ) {
    searchPrefix( reg.pattern(), 0, maxFilterResults );
    return;
  }

  // If the headword is already finished loaded, do nothing
  if ( finished ) {
    return;
  }

  // Back to normal state, restore the original model
  if ( reg.pattern().isEmpty() ) {
    QMutexLocker _( &lock );
    if ( !filtering ) {
      return;
    }
    filtering = false;
    currentSearchPrefix.clear();

    beginResetModel();
    words = QStringList( original_words );
    endResetModel();

    return;
  }
  else {
    QMutexLocker _( &lock );
    if ( !filtering ) {
      filtering      = true;
      original_words = QStringList( words );
    }
  }

  filterWords.clear();
  auto sr = _dict->prefixMatch( Text::removeTrailingZero( reg.pattern() ), maxFilterResults );
  connect( sr.get(), &Dictionary::Request::finished, this, [ this, sr ]() {
    requestFinished( sr );
  } );
}

void HeadwordListModel::appendWord( const QString & word )
{
  hashedWords.insert( word );
  words.append( word );
}

void HeadwordListModel::requestFinished( const sptr< Dictionary::WordSearchRequest > & request )
{
  if ( request->isFinished() ) {
    if ( !request->getErrorString().isEmpty() ) {
      qDebug() << "error:" << request->getErrorString();
    }
    else if ( request->matchesCount() ) {
      auto allmatches = request->getAllMatches();
      for ( auto & match : allmatches ) {
        filterWords.append( QString::fromStdU32String( match.word ) );
      }
    }
  }

  if ( filterWords.isEmpty() ) {
    return;
  }
  beginResetModel();
  words = QStringList( filterWords );
  endResetModel();
  emit numberPopulated( words.size() );
}

int HeadwordListModel::wordCount() const
{
  return words.size();
}

QVariant HeadwordListModel::data( const QModelIndex & index, int role ) const
{
  if ( !index.isValid() ) {
    return {};
  }

  if ( index.row() >= totalSize || index.row() < 0 || index.row() >= words.size() ) {
    return {};
  }

  if ( role == Qt::DisplayRole ) {
    return words.at( index.row() );
  }
  return {};
}

bool HeadwordListModel::canFetchMore( const QModelIndex & parent ) const
{
  if ( parent.isValid() || filtering || finished ) {
    return false;
  }
  return ( words.size() < totalSize );
}

void HeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if ( parent.isValid() || filtering || finished ) {
    return;
  }

  // Try to use headword index first
  if ( useIndex ) {
    if ( fetchFromIndex( currentOffset, HEADWORD_PAGE_SIZE ) ) {
      return;
    }
  }

  // Fallback to legacy btree-based fetching
  fetchFromBtree();
}

bool HeadwordListModel::fetchFromIndex( int offset, int limit )
{
  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( !btreeDict ) {
    return false;
  }

  auto * hwIndex = btreeDict->getHeadwordIndex();
  if ( !hwIndex ) {
    return false;
  }

  HeadwordIndex::PagedResult result;

  if ( currentSearchPrefix.isEmpty() ) {
    result = hwIndex->getPage( offset, limit );
  }
  else {
    result = hwIndex->searchPrefix( currentSearchPrefix, offset, limit );
  }

  if ( result.headwords.isEmpty() ) {
    finished = true;
    return true;
  }

  beginInsertRows( QModelIndex(), words.size(), words.size() + result.headwords.size() - 1 );
  for ( const QString & word : std::as_const( result.headwords ) ) {
    words << word;
    hashedWords.insert( word );
  }
  endInsertRows();

  currentOffset += result.headwords.size();

  if ( !result.hasMore ) {
    finished = true;
  }

  emit numberPopulated( words.size() );
  return true;
}

void HeadwordListModel::fetchFromBtree()
{
  // For small dictionaries, load all at once
  if ( totalSize < HEADWORDS_MAX_LIMIT ) {
    finished = true;

    beginInsertRows( QModelIndex(), 0, totalSize - 1 );
    _dict->getHeadwords( words );
    endInsertRows();

    emit numberPopulated( words.size() );
    return;
  }

  // For large dictionaries, load in batches
  QSet< QString > headword;
  QMutexLocker _( &lock );

  _dict->findHeadWordsWithLenth( index, &headword, HEADWORD_PAGE_SIZE );
  if ( headword.isEmpty() ) {
    finished = true;
    return;
  }

  beginInsertRows( QModelIndex(), words.size(), words.size() + headword.count() - 1 );
  for ( const auto & word : std::as_const( headword ) ) {
    words << word;
  }
  endInsertRows();

  emit numberPopulated( words.size() );
}

int HeadwordListModel::getCurrentIndex() const
{
  return index;
}

bool HeadwordListModel::containWord( const QString & word )
{
  return hashedWords.contains( word );
}

void HeadwordListModel::setMaxFilterResults( int _maxFilterResults )
{
  this->maxFilterResults = _maxFilterResults;
}

QSet< QString > HeadwordListModel::getRemainRows( int & nodeIndex )
{
  // If using index, fetch remaining pages
  if ( useIndex ) {
    auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
    if ( btreeDict ) {
      auto * hwIndex = btreeDict->getHeadwordIndex();
      if ( hwIndex ) {
        auto result = hwIndex->getPage( nodeIndex, 10000 );
        nodeIndex += result.headwords.size();

        QSet< QString > filtered;
        for ( const QString & word : std::as_const( result.headwords ) ) {
          if ( !containWord( word ) ) {
            filtered.insert( word );
          }
        }
        return filtered;
      }
    }
  }

  // Fallback to legacy method
  QSet< QString > headword;
  QMutexLocker _( &lock );
  _dict->findHeadWordsWithLenth( nodeIndex, &headword, 10000 );

  QSet< QString > filtered;
  for ( const auto & word : std::as_const( headword ) ) {
    if ( !containWord( word ) ) {
      filtered.insert( word );
    }
  }
  return filtered;
}

void HeadwordListModel::setDict( Dictionary::Class * dict )
{
  _dict         = dict;
  totalSize     = _dict->getWordCount();
  currentOffset = 0;
  useIndex      = false;

  // Check if headword index is available
  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( btreeDict && btreeDict->haveHeadwordIndex() ) {
    auto * hwIndex = btreeDict->getHeadwordIndex();
    if ( hwIndex ) {
      useIndex  = true;
      totalSize = hwIndex->getTotalCount();
      qDebug() << "Using headword index for" << QString::fromStdString( dict->getName() );
    }
  }

  // Notify if index is recommended for large dictionaries
  if ( !useIndex && isLargeDictionary() ) {
    qDebug() << "Headword index recommended for" << QString::fromStdString( dict->getName() ) << "(" << totalSize
             << "headwords)";
    emit indexBuildRecommended();
  }
}

bool HeadwordListModel::isLargeDictionary() const
{
  return totalSize > HEADWORDS_MAX_LIMIT;
}

bool HeadwordListModel::isIndexBuildRecommended() const
{
  return !useIndex && isLargeDictionary();
}

bool HeadwordListModel::isIndexBuilding() const
{
  return Utils::AtomicInt::loadAcquire( indexBuilding ) != 0;
}

void HeadwordListModel::cancelIndexBuild()
{
  indexBuildCancelled.ref();
}

bool HeadwordListModel::hasHeadwordIndex() const
{
  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( !btreeDict ) {
    return false;
  }
  return btreeDict->haveHeadwordIndex();
}

void HeadwordListModel::buildHeadwordIndex( bool autoBuild )
{
  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( !btreeDict ) {
    emit indexBuildFinished( false );
    return;
  }

  // Don't build if already have index or already building
  if ( btreeDict->haveHeadwordIndex() || isIndexBuilding() ) {
    emit indexBuildFinished( btreeDict->haveHeadwordIndex() );
    return;
  }

  // Reset cancellation flag
  while ( Utils::AtomicInt::loadAcquire( indexBuildCancelled ) ) {
    indexBuildCancelled.deref();
  }

  indexBuilding.ref();

  qDebug() << "Building headword index for" << QString::fromStdString( _dict->getName() )
           << ( autoBuild ? "(auto)" : "(manual)" );

  QThreadPool::globalInstance()->start( [ this, btreeDict ]() {
    btreeDict->makeHeadwordIndex( indexBuildCancelled );

    bool success = false;

    // Update model to use index if build succeeded
    if ( btreeDict->haveHeadwordIndex() ) {
      auto * hwIndex = btreeDict->getHeadwordIndex();
      if ( hwIndex ) {
        useIndex  = true;
        totalSize = hwIndex->getTotalCount();
        success   = true;
        qDebug() << "Headword index built successfully:" << totalSize << "headwords";
      }
    }

    // Clear building flag
    while ( Utils::AtomicInt::loadAcquire( indexBuilding ) ) {
      indexBuilding.deref();
    }

    emit indexBuildFinished( success );
  } );
}

void HeadwordListModel::searchPrefix( const QString & prefix, int offset, int limit )
{
  if ( !useIndex ) {
    return;
  }

  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( !btreeDict ) {
    return;
  }

  auto * hwIndex = btreeDict->getHeadwordIndex();
  if ( !hwIndex ) {
    return;
  }

  currentSearchPrefix = prefix;

  auto result = hwIndex->searchPrefix( prefix, offset, limit );

  beginResetModel();
  words.clear();
  for ( const QString & word : std::as_const( result.headwords ) ) {
    words << word;
  }
  endResetModel();

  filtering     = true;
  currentOffset = result.headwords.size();
  finished      = !result.hasMore;

  emit numberPopulated( words.size() );
}

void HeadwordListModel::searchWildcard( const QString & pattern, int offset, int limit )
{
  if ( !useIndex ) {
    return;
  }

  auto * btreeDict = dynamic_cast< BtreeIndexing::BtreeDictionary * >( _dict );
  if ( !btreeDict ) {
    return;
  }

  auto * hwIndex = btreeDict->getHeadwordIndex();
  if ( !hwIndex ) {
    return;
  }

  auto result = hwIndex->searchWildcard( pattern, offset, limit );

  beginResetModel();
  words.clear();
  for ( const QString & word : std::as_const( result.headwords ) ) {
    words << word;
  }
  endResetModel();

  filtering     = true;
  currentOffset = result.headwords.size();
  finished      = !result.hasMore;

  emit numberPopulated( words.size() );
}
