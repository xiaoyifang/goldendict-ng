#include "resourcerequest.hh"

std::shared_ptr< ResourceRequest > ResourceRequest::NoDataFinished( bool succeed )
{
  auto ret        = std::make_shared< ResourceRequest >();
  ret->hasAnyData = succeed;
  ret->finish();
  return ret;
}

void ResourceRequest::cancel()
{
  f.cancel();
}

void ResourceRequest::finish()
{
  f.waitForFinished();
  emit finished();
}

bool ResourceRequest::isFinished()
{
  return f.isCanceled() || f.isFinished();
}

qsizetype ResourceRequest::dataSize()
{
  return data.size();
}

void ResourceRequest::setErrorString( const QString & str )
{
  this->errorString = str;
}

void ResourceRequest::appendString( std::string_view str )
{
  QMutexLocker _( &dataMutex );
  data.reserve( data.size() + str.size() );
  data.insert( data.end(), str.begin(), str.end() );
}

void ResourceRequest::appendDataSlice( const void * buffer, size_t size )
{
  QMutexLocker _( &dataMutex );

  size_t offset = data.size();

  data.resize( data.size() + size );

  memcpy( &data.front() + offset, buffer, size );
}

void ResourceRequest::getDataSlice( size_t offset, size_t size, void * buffer )
{
  if ( size == 0 ) {
    return;
  }
  QMutexLocker _( &dataMutex );

  if ( !hasAnyData ) {
    throw std::runtime_error( "index out of range" ); // FIXME: shuold we follow old GD?
  }

  memcpy( buffer, &data[ offset ], size );
}

std::vector< char > & ResourceRequest::getFullData()
{
  if ( !isFinished() ) {
    throw std::runtime_error( "Full data not finished" );
  }

  return data;
}

void ResourceRequest::update()
{
  emit updated();
}

ResourceRequest::~ResourceRequest()
{
  f.waitForFinished();
}
