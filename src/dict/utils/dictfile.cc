/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictfile.hh"

#include "zipfile.hh"

#include <string>
#ifdef __WIN32
  #include <windows.h>
#endif
#include "utils.hh"

namespace File {

bool tryPossibleName( std::string const & name, std::string & copyTo )
{
  if ( Utils::Fs::exists( name ) ) {
    copyTo = name;
    return true;
  }
  else {
    return false;
  }
}

bool tryPossibleZipName( std::string const & name, std::string & copyTo )
{
  if ( ZipFile::SplitZipFile( name.c_str() ).exists() ) {
    copyTo = name;
    return true;
  }
  else {
    return false;
  }
}

void loadFromFile( std::string const & filename, std::vector< char > & data )
{
  File::Index f( filename, QIODevice::ReadOnly );
  auto size = f.file().size(); // QFile::size() obtains size via statx on Linux
  data.resize( size );
  f.read( data.data(), size );
}

Index::Index( std::string_view filename, QIODevice::OpenMode mode )
{
  f.setFileName( QString::fromUtf8( filename.data(), filename.size() ) );
  if ( !f.open( mode ) ) {
    throw exCantOpen( ( f.fileName() + ": " + f.errorString() ).toStdString() );
  }
}

void Index::read( void * buf, qint64 size )
{
  if ( f.read( static_cast< char * >( buf ), size ) != size ) {
    throw exReadError();
  }
}

size_t Index::readRecords( void * buf, qint64 size, qint64 count )
{
  qint64 result = f.read( static_cast< char * >( buf ), size * count );
  return result < 0 ? result : result / size;
}

void Index::write( void const * buf, qint64 size )
{
  if ( 0 == size ) {
    return;
  }

  if ( size < 0 ) {
    throw exWriteError();
  }

  f.write( static_cast< char const * >( buf ), size );
}

size_t Index::writeRecords( void const * buf, qint64 size, qint64 count )
{
  qint64 result = f.write( static_cast< const char * >( buf ), size * count );
  return result < 0 ? result : result / size;
}

char * Index::gets( char * s, int size, bool stripNl )
{
  qint64 len    = f.readLine( s, size );
  char * result = len > 0 ? s : nullptr;

  if ( result && stripNl ) {

    char * last = result + len;

    while ( len-- ) {
      --last;

      if ( *last == '\n' || *last == '\r' ) {
        *last = 0;
      }
      else {
        break;
      }
    }
  }

  return result;
}

std::string Index::gets( bool stripNl )
{
  char buf[ 1024 ];

  if ( !gets( buf, sizeof( buf ), stripNl ) ) {
    throw exReadError();
  }

  return { buf };
}

QByteArray Index::readall()
{
  return f.readAll();
};


void Index::seek( qint64 offset )
{
  if ( !f.seek( offset ) ) {
    throw exSeekError();
  }
}

uchar * Index::map( qint64 offset, qint64 size )
{
  return f.map( offset, size );
}

bool Index::unmap( uchar * address )
{
  return f.unmap( address );
}


void Index::seekEnd()
{
  if ( !f.seek( f.size() ) ) {
    throw exSeekError();
  }
}

void Index::rewind()
{
  seek( 0 );
}

qint64 Index::tell()
{
  return f.pos();
}

bool Index::eof() const
{
  return f.atEnd();
}

QFile & Index::file()
{
  return f;
}

void Index::close()
{
  f.close();
}

Index::~Index() noexcept
{
  f.close();
}


} // namespace File
