#include "dict/dictionary_requests.hh"
#include "common/utils.hh"

namespace Request {

bool Base::isFinished()
{
  return Utils::AtomicInt::loadAcquire( isFinishedFlag );
}

void Base::update()
{
  if ( !Utils::AtomicInt::loadAcquire( isFinishedFlag ) )
    emit updated();
}

void Base::finish()
{
  if ( !Utils::AtomicInt::loadAcquire( isFinishedFlag ) ) {
    isFinishedFlag.ref();

    emit finished();
  }
}

void Base::setErrorString( QString const & str )
{
  QMutexLocker _( &errorStringMutex );

  errorString = str;
}

QString Base::getErrorString()
{
  QMutexLocker _( &errorStringMutex );

  return errorString;
}


///////// WordSearchRequest

size_t WordSearch::matchesCount()
{
  QMutexLocker _( &dataMutex );

  return matches.size();
}

WordMatch WordSearch::operator[]( size_t index )
{
  QMutexLocker _( &dataMutex );

  if ( index >= matches.size() )
    throw exIndexOutOfRange();

  return matches[ index ];
}

std::vector< WordMatch > & WordSearch::getAllMatches()
{
  if ( !isFinished() )
    throw exRequestUnfinished();

  return matches;
}

void WordSearch::addMatch( WordMatch const & match )
{
  unsigned n;
  for ( n = 0; n < matches.size(); n++ )
    if ( matches[ n ].word.compare( match.word ) == 0 )
      break;

  if ( n >= matches.size() )
    matches.push_back( match );
}

////////////// DataRequest

long Blob::dataSize()
{
  QMutexLocker _( &dataMutex );

  return hasAnyData ? (long)data.size() : -1;
}

void Blob::appendDataSlice( const void * buffer, size_t size )
{
  QMutexLocker _( &dataMutex );

  size_t offset = data.size();

  data.resize( data.size() + size );

  memcpy( &data.front() + offset, buffer, size );
}

void Blob::getDataSlice( size_t offset, size_t size, void * buffer )
{
  if ( size == 0 )
    return;

  QMutexLocker _( &dataMutex );

  if ( !hasAnyData )
    throw exSliceOutOfRange();

  memcpy( buffer, &data[ offset ], size );
}

std::vector< char > & Blob::getFullData()
{
  if ( !isFinished() )
    throw exRequestUnfinished();

  return data;
}

std::vector< char > & Article::getFullData()
{
  auto * result = new std::vector< char >( data.begin(), data.end() );
  return *result;
}

long Article::dataSize()
{
  return data.size();
}

void Article::appendStrToData( const QStringView & s )
{
  hasAnyData = true;
  data.append( s.toUtf8().data() );
}

void Article::appendStrToData( const std::string_view & s )
{
  hasAnyData = true;
  data.append( s );
}

void Article::getDataSlice( size_t offset, size_t size, void * buffer )
{
  if ( size == 0 )
    return;

  QMutexLocker _( &dataMutex );

  if ( !hasAnyData )
    throw exSliceOutOfRange();

  memcpy( buffer, &data[ offset ], size );
}

} // namespace Request