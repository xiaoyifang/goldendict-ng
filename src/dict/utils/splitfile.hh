#pragma once

#include <QFile>
#include <QList>
#include <QString>

#include <vector>
#include <string>

namespace SplitFile {

using std::vector;
using std::string;

// Class for work with split files

class SplitFile
{
protected:

  QList< QFile * > files;
  QList< quint64 > offsets;
  // TODO: rename currentFile (?)
  int currentFile;

  void appendFile( const QString & name );

public:

  SplitFile();
  ~SplitFile();

  virtual void setFileName( const QString & name ) = 0;
  void getFilenames( vector< string > & names ) const;
  int getCurrentFile() const;
  bool open( QFile::OpenMode mode );
  void close();
  bool seek( quint64 pos );
  qint64 read( char * data, qint64 maxSize );
  QByteArray read( qint64 maxSize );
  bool getChar( char * c );
  qint64 size() const
  {
    return files.isEmpty() ? 0 : offsets.last() + files.last()->size();
  }
  bool exists() const
  {
    return !files.isEmpty();
  }
  qint64 pos() const;
};

} // namespace SplitFile
