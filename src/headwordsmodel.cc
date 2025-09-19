#include "headwordsmodel.hh"
#include "dict/xapianidx.hh"
#include "text.hh"

HeadwordListModel::HeadwordListModel( QObject * parent ):
  QAbstractListModel( parent ),
  filtering( false ),
  totalSize( 0 ),
  finished( false ),
  index( 0 ),
  ptr( nullptr ),
  maxFilterResults( 1000 )
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
  return finished || ( words.size() >= totalSize ) || ( totalSize == 0 );
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
  // If headwords are already fully loaded, do nothing
  if ( finished ) {
    return;
  }
  // Back to normal state, restore the original model
  if ( reg.pattern().isEmpty() ) {
    QMutexLocker _( &lock );
    // Race condition
    if ( !filtering ) {
      return;
    }
    filtering = false;

    // Reset to previous models
    beginResetModel();
    words = QStringList( original_words );
    endResetModel();

    return;
  }
  else {
    QMutexLocker _( &lock );
    // First time entering filtering mode
    if ( !filtering ) {
      filtering      = true;
      original_words = QStringList( words );
    }
  }
  filterWords.clear();
  
  // Use xapianidx for searching, supporting wildcard and prefix search
  std::string dictId = _dict->getId();
  std::string searchQuery = Text::removeTrailingZero(reg.pattern()).toStdString();
  uint32_t totalCount = 0;
  uint32_t offset = 0;
  
  try {
    auto searchResults = XapianIndexing::searchIndexedWordsByOffset(dictId, searchQuery, offset, maxFilterResults, totalCount);
    
    // Process search results
    sptr<Dictionary::WordSearchRequestInstant> sr = std::make_shared<Dictionary::WordSearchRequestInstant>();
    for (const auto& pair : searchResults) {
      sr->addMatch(Dictionary::WordMatch(QStringView(pair.first).toU32String()));
    }
    
    // Process search results directly without asynchronous waiting
    requestFinished(sr);
  } catch (const std::exception& e) {
    qDebug() << "Xapian search error:" << e.what();
  }
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
  return ( words.size() < totalSize ) && ( totalSize > 0 );
}

void HeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if ( parent.isValid() || filtering || finished ) {
    return;
  }

  // Arbitrary number threshold
  if ( totalSize < HEADWORDS_MAX_LIMIT && totalSize > 0 ) {
    finished = true;

    beginInsertRows( QModelIndex(), 0, totalSize - 1 );
    _dict->getHeadwords( words );

    endInsertRows();
    emit numberPopulated( words.size() );
    return;
  }

  // Get batch headwords from Xapian index
  QSet< QString > headword;
  QMutexLocker _( &lock );

  // Get dictionary ID
  std::string dictId = _dict->getId();
  
  // Calculate current offset (0-based)
  uint32_t offset = words.size();
  uint32_t totalFromXapian = 0;
  
  // Use XapianIndexing::getIndexedWordsByOffset with dictId
  auto indexedWords = XapianIndexing::getIndexedWordsByOffset( dictId, offset, 1000, totalFromXapian );
  
  // If this is the first fetch, update totalSize
  if ( totalSize == 0 && totalFromXapian > 0 ) {
    totalSize = totalFromXapian;
  }
  
  // Check if all content has been fetched
  if ( words.size() + indexedWords.size() >= totalSize ) {
    finished = true;
  }
  
  // Populate results
  if ( !indexedWords.empty() ) {
    beginInsertRows( QModelIndex(), words.size(), words.size() + indexedWords.size() - 1 );
    for ( const auto & [word, count] : indexedWords ) {
      if ( !hashedWords.contains( word ) ) {
        words << word;
        hashedWords.insert( word );
      }
    }
    endInsertRows();
    emit numberPopulated( words.size() );
  }
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
  QSet< QString > headword;
  QMutexLocker _( &lock );
  
  // Get remaining headwords using Xapian index
  std::string dictId = _dict->getId();
  uint32_t totalFromXapian = 0;
  
  // Use XapianIndexing::getIndexedWordsByOffset with dictId
  auto indexedWords = XapianIndexing::getIndexedWordsByOffset( dictId, nodeIndex, 10000, totalFromXapian );
  
  // Collect results
  for ( const auto & [word, count] : indexedWords ) {
    headword.insert( word );
  }
  
  // Update nodeIndex
  nodeIndex += headword.size();
  
  // Filter out existing words
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
  _dict     = dict;
  totalSize = _dict->getWordCount();
}
