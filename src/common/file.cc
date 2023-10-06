/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "file.hh"

#include "zipfile.hh"

#include <string>
#include <QFileInfo>
#ifdef __WIN32
  #include <windows.h>
#endif

namespace File {

bool tryPossibleName( std::string const & name, std::string & copyTo )
{
  if ( File::exists( name ) ) {
    copyTo = name;
    return true;
  }
  else
    return false;
}

bool tryPossibleZipName( std::string const & name, std::string & copyTo )
{
  if ( ZipFile::SplitZipFile( name.c_str() ).exists() ) {
    copyTo = name;
    return true;
  }
  else
    return false;
}

void loadFromFile( std::string const & filename, std::vector< char > & data )
{
  File::Class f( filename, "rb" );
  auto size = f.file().size(); // QFile::size() obtains size via statx on Linux
  data.resize( size );
  f.read( data.data(), size );
}

void Class::open( char const * mode )
{
  QFile::OpenMode openMode = QIODevice::Text;

  const char * pch = mode;
  while ( *pch ) {
    switch ( *pch ) {
      case 'r':
        openMode |= QIODevice::ReadOnly;
        break;
      case 'w':
        openMode |= QIODevice::WriteOnly;
        break;
      case '+':
        openMode &= ~( QIODevice::ReadOnly | QIODevice::WriteOnly );
        openMode |= QIODevice::ReadWrite;
        break;
      case 'a':
        openMode |= QIODevice::Append;
        break;
      case 'b':
        openMode &= ~QIODevice::Text;
        break;
      default:
        break;
    }
    ++pch;
  }

  if ( !f.open( openMode ) )
    throw exCantOpen( f.fileName().toStdString() + ": " + f.errorString().toUtf8().data() );
}

Class::Class( std::string_view filename, char const * mode )
{
  f.setFileName( QString::fromUtf8( filename.data(), filename.size() ) );
  open( mode );
}

void Class::read( void * buf, qint64 size )
{
  if ( f.read( static_cast< char * >( buf ), size ) != size ) {
    throw exReadError();
  }
}

size_t Class::readRecords( void * buf, qint64 size, qint64 count )
{
  qint64 result = f.read( static_cast< char * >( buf ), size * count );
  return result < 0 ? result : result / size;
}

void Class::write( void const * buf, qint64 size )
{
  if ( 0 == size ) {
    return;
  }

  if ( size < 0 ) {
    throw exWriteError();
  }

  f.write( static_cast< char const * >( buf ), size );
}

size_t Class::writeRecords( void const * buf, qint64 size, qint64 count )
{
  qint64 result = f.write( static_cast< const char * >( buf ), size * count );
  return result < 0 ? result : result / size;
}

char * Class::gets( char * s, int size, bool stripNl )
{
  qint64 len    = f.readLine( s, size );
  char * result = len > 0 ? s : nullptr;

  if ( result && stripNl ) {

    char * last = result + len;

    while ( len-- ) {
      --last;

      if ( *last == '\n' || *last == '\r' )
        *last = 0;
      else
        break;
    }
  }

  return result;
}

std::string Class::gets( bool stripNl )
{
  char buf[ 1024 ];

  if ( !gets( buf, sizeof( buf ), stripNl ) )
    throw exReadError();

  return { buf };
}

QByteArray Class::readall()
{
  return f.readAll();
};


void Class::seek( qint64 offset )
{
  if ( !f.seek( offset ) )
    throw exSeekError();
}

uchar * Class::map( qint64 offset, qint64 size )
{
  return f.map( offset, size );
}

bool Class::unmap( uchar * address )
{
  return f.unmap( address );
}


void Class::seekEnd()
{
  if ( !f.seek( f.size() ) )
    throw exSeekError();
}

void Class::rewind()
{
  seek( 0 );
}

qint64 Class::tell()
{
  return f.pos();
}

bool Class::eof() const
{
  return f.atEnd();
}

QFile & Class::file()
{
  return f;
}

void Class::close()
{
  f.close();
}

Class::~Class() noexcept
{
  f.close();
}


} // namespace File
