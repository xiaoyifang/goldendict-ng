/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ex.hh"

#include <QFile>
#include <QFileInfo>
#include <cstdio>
#include <string>
#include <vector>
#include <QMutex>
#include <type_traits>
#include <string>

/// File utilities
namespace File {

DEF_EX( Ex, "File exception", std::exception )
DEF_EX_STR( exCantOpen, "Can't open", Ex )
DEF_EX( exReadError, "Error reading from the file", Ex )
DEF_EX( exWriteError, "Error writing to the file", Ex )
DEF_EX( exSeekError, "File seek error", Ex )
DEF_EX( exAllocation, "Memory allocation error", Ex )

bool tryPossibleName( const std::string & name, std::string & copyTo );

bool tryPossibleZipName( const std::string & name, std::string & copyTo );

void loadFromFile( const std::string & filename, std::vector< char > & data );


/// Exclusivly used for processing GD's index files
class Index
{
  QFile f;

public:
  QMutex lock;

  // Create QFile Object and open() it.
  Index( std::string_view filename, QIODevice::OpenMode mode );

  /// QFile::read  & QFile::write , but with exception throwing
  void read( void * buf, qint64 size );
  void write( const void * buf, qint64 size );

  // Read sizeof(T) bytes and convert the read data to T
  template< typename T >
  T read()
  {
    T value;
    readType( value );
    return value;
  }

  template< typename T >
  void write( const T & value )
  {
    write( &value, sizeof( value ) );
  }

  /// Attempts reading at most 'count' records sized 'size'. Returns
  /// the number of records it managed to read, up to 'count'.
  size_t readRecords( void * buf, qint64 size, qint64 count );

  /// Attempts writing at most 'count' records sized 'size'. Returns
  /// the number of records it managed to write, up to 'count'.
  /// This function does not employ buffering, but flushes the buffer if it
  /// was used before.
  size_t writeRecords( const void * buf, qint64 size, qint64 count );

  /// Read a line with option to strip the trailing newline character
  /// Returns either s or 0 if no characters were read.
  char * gets( char * s, int size, bool stripNl = false );

  /// Like the above, but uses its own local internal buffer and strips newlines by default.
  std::string gets( bool stripNl = true );

  /// Read 32bit as uint, then reading the subsequent data into a container
  template< typename T >
  void readU32SizeAndData( T & container )
  {
    static_assert( std::is_same< T, std::vector< unsigned char > >::value
                     || std::is_same< T, std::vector< char > >::value || std::is_same< T, std::string >::value,
                   "T must be either std::vector<char> or std::string" );
    uint32_t size = 0;
    read( &size, sizeof( uint32_t ) );
    if ( size > 0 ) {
      container.resize( size );
      read( container.data(), size );
    }
  };

  /// export QFile::readall
  QByteArray readall();

  /// export QFile::seek
  void seek( qint64 offset );

  /// Seeks to the end of file.
  void seekEnd();

  /// Seeks to the beginning of file
  void rewind();

  /// Tells the current position within the file, relative to its beginning.
  qint64 tell();

  /// QFile::atEnd() const
  bool eof() const;

  uchar * map( qint64 offset, qint64 size );
  bool unmap( uchar * address );


  /// Returns the underlying QFile* , so other operations can be
  /// performed on it.
  QFile & file();

  /// Closes the file. No further operations are valid.
  void close();

  ~Index() noexcept;

private:

  template< typename T >
  void readType( T & value )
  {
    read( &value, sizeof( value ) );
  }
};

} // namespace File
