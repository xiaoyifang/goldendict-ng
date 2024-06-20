#include "headwordsmodel.hh"
#include "wstring_qt.hh"

HeadwordListModel::HeadwordListModel( QObject * parent ):
  QAbstractListModel( parent ),
  filtering( false ),
  totalSize( 0 ),
  finished( false ),
  index( 0 ),
  ptr( nullptr )
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

void HeadwordListModel::setFilter( QRegularExpression reg )
{
  if ( finished ) {
    return;
  }
  if ( reg.pattern().isEmpty() ) {
    filtering = false;

    //reset to previous models
    beginResetModel();
    words.clear();
    words << original_words;
    endResetModel();
    original_words.clear();

    return;
  }
  filtering = true;
  filterWords.clear();
  auto sr = _dict->prefixMatch( gd::removeTrailingZero( reg.pattern() ), maxFilterResults );
  connect( sr.get(), &Dictionary::Request::finished, this, &HeadwordListModel::requestFinished, Qt::QueuedConnection );
  queuedRequests.push_back( sr );
}

void HeadwordListModel::appendWord( const QString & word )
{
  hashedWords.insert( word );
  words.append( word );
}

void HeadwordListModel::requestFinished()
{
  // See how many new requests have finished, and if we have any new results
  for ( auto i = queuedRequests.begin(); i != queuedRequests.end(); ) {
    if ( ( *i )->isFinished() ) {
      if ( !( *i )->getErrorString().isEmpty() ) {
        qDebug() << "error:" << ( *i )->getErrorString();
      }
      else if ( ( *i )->matchesCount() ) {
        auto allmatches = ( *i )->getAllMatches();
        for ( auto & match : allmatches )
          filterWords.append( QString::fromStdU32String( match.word ) );
      }
      queuedRequests.erase( i++ );
    }
    else
      ++i;
  }

  if ( queuedRequests.empty() ) {
    if ( filterWords.isEmpty() ) {
      return;
    }
    beginResetModel();

    original_words << words;
    words.clear();
    words << filterWords;

    endResetModel();
  }
}

int HeadwordListModel::wordCount() const
{
  return words.size();
}

QVariant HeadwordListModel::data( const QModelIndex & index, int role ) const
{
  if ( !index.isValid() )
    return {};

  if ( index.row() >= totalSize || index.row() < 0 || index.row() >= words.size() )
    return {};

  if ( role == Qt::DisplayRole ) {
    return words.at( index.row() );
  }
  return {};
}

bool HeadwordListModel::canFetchMore( const QModelIndex & parent ) const
{
  if ( parent.isValid() || filtering || finished )
    return false;
  return ( words.size() < totalSize );
}

void HeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if ( parent.isValid() || filtering || finished )
    return;

  //arbitrary number
  if ( totalSize < 500000 ) {
    finished = true;

    beginInsertRows( QModelIndex(), 0, totalSize - 1 );
    _dict->getHeadwords( words );

    endInsertRows();
    emit numberPopulated( words.size() );
    return;
  }


  QSet< QString > headword;
  QMutexLocker _( &lock );

  _dict->findHeadWordsWithLenth( index, &headword, 1000 );
  if ( headword.isEmpty() ) {
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
  QSet< QString > headword;
  QMutexLocker _( &lock );
  _dict->findHeadWordsWithLenth( nodeIndex, &headword, 10000 );

  QSet< QString > filtered;
  for ( const auto & word : headword ) {
    if ( !containWord( word ) )
      filtered.insert( word );
  }
  return filtered;
}

void HeadwordListModel::setDict( Dictionary::Class * dict )
{
  _dict     = dict;
  totalSize = _dict->getWordCount();
  maxElementSizePerNode = sqrt( totalSize ) + 1;
}
