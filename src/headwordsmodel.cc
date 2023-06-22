#include "headwordsmodel.hh"
#include "wstring_qt.hh"

HeadwordListModel::HeadwordListModel( QObject * parent ) :
  QAbstractListModel( parent ), filtering( false ), totalSize(0), index( 0 ),ptr( nullptr )
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
  return words.size() >= totalSize;
}

// export headword
QString HeadwordListModel::getRow( int row )
{
  if( fileSortedList.empty() )
  {
    fileSortedList << words;
    fileSortedList.sort();
  }
  return fileSortedList.at( row );
}

void HeadwordListModel::setFilter( QRegularExpression reg )
{
  if( reg.pattern().isEmpty() )
  {
    filtering = false;
    return;
  }
  filtering = true;
  filterWords.clear();
  auto sr = _dict->prefixMatch( gd::removeTrailingZero( reg.pattern() ), 500 );
  connect( sr.get(), &Request::Base::finished, this, &HeadwordListModel::requestFinished, Qt::QueuedConnection );
  queuedRequests.push_back( sr );
}

void HeadwordListModel::appendWord( const QString & word )
{
  hashedWords.insert( word );
  words.append( word );
}

void HeadwordListModel::addMatches( QStringList matches)
{
  QStringList filtered;
  for ( auto const & w : matches ) {
    if ( !containWord( w ) ) {
      filtered << w;
    }
  }

  if ( filtered.isEmpty() )
    return;

  beginInsertRows( QModelIndex(), words.size(), words.size() + filtered.count() - 1 );
  for ( const auto & word : filtered ) {
    appendWord( word );
  }
  endInsertRows();
}

void HeadwordListModel::requestFinished()
{
  // See how many new requests have finished, and if we have any new results
  for( auto i = queuedRequests.begin();
       i != queuedRequests.end(); )
  {
    if( ( *i )->isFinished() )
    {
      if( !( *i )->getErrorString().isEmpty() )
      {
        qDebug() << "error:" << ( *i )->getErrorString();
      }

      if( ( *i )->matchesCount() )
      {
        auto allmatches = ( *i )->getAllMatches();
        for( auto & match : allmatches )
          filterWords.append( QString::fromStdU32String( match.word ) );
      }
      queuedRequests.erase( i++ );
    }
    else
      ++i;
  }

  if( queuedRequests.empty() )
  {
    QStringList filtered;
    for( auto & w : filterWords )
    {
      if( !containWord( w ) )
      {
        filtered << w;
      }
    }
    if( filtered.isEmpty() )
      return;

    beginInsertRows( QModelIndex(), words.size(), words.size() + filtered.count() - 1 );
    for( const auto & word : filtered )
      appendWord( word );
    endInsertRows();
  }
}

int HeadwordListModel::wordCount() const
{
  return words.size();
}

QVariant HeadwordListModel::data( const QModelIndex & index, int role ) const
{
  if( !index.isValid() )
    return {};

  if( index.row() >= totalSize || index.row() < 0 || index.row() >= words.size() )
    return {};

  if( role == Qt::DisplayRole )
  {
    return words.at( index.row() );
  }
  return {};
}

bool HeadwordListModel::canFetchMore( const QModelIndex & parent ) const
{
  if( parent.isValid() || filtering )
    return false;
  return ( words.size() < totalSize );
}

void HeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if( parent.isValid() || filtering )
    return;

  QSet< QString > headword;
  QMutexLocker _( &lock );

  _dict->findHeadWordsWithLenth( index, &headword, 1000 );
  if( headword.isEmpty() )
  {
    return;
  }

  QSet< QString > filtered;
  for( const auto & word : qAsConst( headword ) )
  {
    if( !containWord( word ) )
      filtered.insert( word );
  }

  beginInsertRows( QModelIndex(), words.size(), words.size() + filtered.count() - 1 );
  for( const auto & word : filtered )
  {
    appendWord( word );
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

QSet< QString > HeadwordListModel::getRemainRows( int & nodeIndex )
{
  QSet< QString > headword;
  QMutexLocker _( &lock );
  _dict->findHeadWordsWithLenth( nodeIndex, &headword, 10000 );

  QSet< QString > filtered;
  for( const auto & word : headword )
  {
    if( !containWord( word ) )
      filtered.insert( word );
  }
  return filtered;
}

void HeadwordListModel::setDict( Dictionary::Class * dict )
{
  _dict     = dict;
  totalSize = _dict->getWordCount();
}
