/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef GOLDENDICT_FILE_HH
#define GOLDENDICT_FILE_HH

#include "ex.hh"

#include <QFile>
#include <QFileInfo>
#include <cstdio>
#include <string>
#include <vector>
#include <QMutex>

/// A wrapper over QFile with some GD specific functions
/// and exception throwing which are required for older coded to work correctly
/// Consider the wrapped QFile as private implementation in the `Pimpl Idiom`
///
/// Note: this is used *only* in code related to `Dictionary::CLass` to manage dict files.
/// In other places, just use QFile directly.
namespace File {

DEF_EX( Ex, "File exception", std::exception )
DEF_EX_STR( exCantOpen, "Can't open", Ex )
DEF_EX( exReadError, "Error reading from the file", Ex )
DEF_EX( exWriteError, "Error writing to the file", Ex )
DEF_EX( exSeekError, "File seek error", Ex )
DEF_EX( exAllocation, "Memory allocation error", Ex )

bool tryPossibleName( std::string const & name, std::string & copyTo );

bool tryPossibleZipName( std::string const & name, std::string & copyTo );

void loadFromFile( std::string const & filename, std::vector< char > & data );

// QFileInfo::exists but used for std::string and char*
inline bool exists( std::string_view filename ) noexcept
{
  return QFileInfo::exists( QString::fromUtf8( filename.data(), filename.size() ) );
};

class Class
{
  QFile f;

public:
  QMutex lock;

  // Create QFile Object and open() it.
  Class( std::string_view filename, char const * mode );

  /// QFile::read  & QFile::write , but with exception throwing
  void read( void * buf, qint64 size );
  void write( void const * buf, qint64 size );

  // Read sizeof(T) bytes and convert the read data to T
  template< typename T >
  T read()
  {
    T value;
    readType( value );
    return value;
  }

  template< typename T >
  void write( T const & value )
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
  size_t writeRecords( void const * buf, qint64 size, qint64 count );

  /// Read a line with option to strip the trailing newline character
  /// Returns either s or 0 if no characters were read.
  char * gets( char * s, int size, bool stripNl = false );

  /// Like the above, but uses its own local internal buffer and strips newlines by default.
  std::string gets( bool stripNl = true );

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

  /// Used in original GD code where in a index file,
  /// a uint32_t representing length followed by a str of that length
  std::string readUInt32WithSubsequentStr();

  template<typename T>
  std::vector< T > readUInt32WithSubsequentVec()
  {
    uint32_t size = 0;
    read( &size, sizeof( uint32_t ) );
    if ( size > 0 ) {
      std::vector< T > ret;
      ret.resize( size );
      read( ret.data(), size );
      return ret;
    }
    return {};
  }

  ~Class() noexcept;

private:
  // QFile::open but with fopen-like mode settings.
  void open( char const * mode );

  template< typename T >
  void readType( T & value )
  {
    read( &value, sizeof( value ) );
  }
};

} // namespace File

#endif
