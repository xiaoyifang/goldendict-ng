/* This file is (c) 2024 xiaoyifang
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "favoriteswal.hh"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QTextStream>

FavoritesWAL::FavoritesWAL( const QString & walFilename, QObject * parent ):
  QObject( parent ),
  m_walFilename( walFilename ),
  m_walFile( walFilename )
{
}

FavoritesWAL::~FavoritesWAL()
{
  if ( m_walFile.isOpen() ) {
    m_walFile.close();
  }
}

bool FavoritesWAL::logAdd( const QStringList & path, bool isFolder )
{
<<<<<<< HEAD
  QJsonObject entry;
  // 使用不同的操作类型字符串来区分普通添加和文件夹添加
  entry[ "op" ]   = isFolder ? "addfolder" : "add";
  entry[ "ts" ]   = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
=======
  QString line = ( isFolder ? "++ " : "+ " ) + serializePath( path );
  return appendEntry( line );
>>>>>>> b587f8ef (1)
}

bool FavoritesWAL::logRemove( const QStringList & path, bool isFolder )
{
<<<<<<< HEAD
  QJsonObject entry;
  // 使用不同的操作类型字符串来区分普通删除和文件夹删除
  entry[ "op" ]   = isFolder ? "removefolder" : "remove";
  entry[ "ts" ]   = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
=======
  QString line = ( isFolder ? "-- " : "- " ) + serializePath( path );
  return appendEntry( line );
>>>>>>> b587f8ef (1)
}

bool FavoritesWAL::logMove( const QStringList & fromPath, const QStringList & toPath, bool isFolder )
{
<<<<<<< HEAD
  QJsonObject entry;
  entry[ "op" ]   = "move";
  entry[ "ts" ]   = QDateTime::currentSecsSinceEpoch();
  entry[ "from" ] = QJsonArray::fromStringList( fromPath );
  entry[ "to" ]   = QJsonArray::fromStringList( toPath );
  // Move操作仍然需要isFolder字段，因为它没有单独的文件夹版本
  entry[ "isFolder" ] = isFolder;

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
=======
  QString line = ( isFolder ? ">> " : "> " ) + serializePath( fromPath ) + " | " + serializePath( toPath );
  return appendEntry( line );
>>>>>>> b587f8ef (1)
}

QList< FavoritesWAL::Entry > FavoritesWAL::replay()
{
  QList< FavoritesWAL::Entry > operations;

  if ( !QFile::exists( m_walFilename ) ) {
    return operations;
  }

  QFile file( m_walFilename );
  if ( !file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    qWarning() << "Failed to open WAL file for replay:" << m_walFilename;
    return operations;
  }

  QTextStream in( &file );
  while ( !in.atEnd() ) {
    QString line = in.readLine().trimmed();
    if ( line.isEmpty() ) {
      continue;
    }

    Entry op = parseEntry( line );
    // Check if operation type is valid (Add is 0, so we need a better check or rely on empty map)
    // However, parseEntry returns Add with empty map on failure.
    // Let's check if the path is empty for Add/Remove/Move operations which should have data.
    if ( op.path.isEmpty() ) {
      qWarning() << "Invalid WAL entry, skipping:" << line;
      continue;
    }

    operations.append( op );
  }

  file.close();
  qDebug() << "WAL replay: loaded" << operations.size() << "operations";
  return operations;
}

bool FavoritesWAL::clear()
{
  if ( QFile::exists( m_walFilename ) ) {
    if ( !QFile::remove( m_walFilename ) ) {
      qWarning() << "Failed to clear WAL file:" << m_walFilename;
      return false;
    }
  }
  return true;
}

bool FavoritesWAL::hasEntries() const
{
  if ( !QFile::exists( m_walFilename ) ) {
    return false;
  }

  QFile file( m_walFilename );
  if ( !file.open( QIODevice::ReadOnly ) ) {
    return false;
  }

  bool hasContent = file.size() > 0;
  file.close();
  return hasContent;
}

bool FavoritesWAL::appendEntry( const QString & line )
{
  // Use QSaveFile for atomic writes
  QSaveFile file( m_walFilename );

  // Read existing content
  QByteArray existingContent;
  if ( QFile::exists( m_walFilename ) ) {
    QFile existing( m_walFilename );
    if ( existing.open( QIODevice::ReadOnly ) ) {
      existingContent = existing.readAll();
      existing.close();
    }
  }

  // Open for writing
  if ( !file.open( QIODevice::WriteOnly | QIODevice::Text ) ) {
    qWarning() << "Failed to open WAL file for writing:" << m_walFilename;
    return false;
  }

  // Write existing content + new entry
  if ( !existingContent.isEmpty() ) {
    file.write( existingContent );
    if ( !existingContent.endsWith( '\n' ) ) {
      file.write( "\n" );
    }
  }
  file.write( line.toUtf8() );
  file.write( "\n" );

  if ( !file.commit() ) {
    qWarning() << "Failed to commit WAL entry:" << file.errorString();
    return false;
  }

  return true;
}

FavoritesWAL::Entry FavoritesWAL::parseEntry( const QString & line )
{
  Entry entry;
  entry.type = Add; // Default

  if ( line.startsWith( "++ " ) ) {
    QString pathStr = line.mid( 3 ).trimmed();
    entry.type      = AddFolder;
    entry.path      = deserializePath( pathStr );
  }
  else if ( line.startsWith( "+ " ) ) {
    QString pathStr = line.mid( 2 ).trimmed();
    entry.type      = Add;
    entry.path      = deserializePath( pathStr );
  }
  else if ( line.startsWith( "-- " ) ) {
    QString pathStr = line.mid( 3 ).trimmed();
    entry.type      = RemoveFolder;
    entry.path      = deserializePath( pathStr );
  }
  else if ( line.startsWith( "- " ) ) {
    QString pathStr = line.mid( 2 ).trimmed();
    entry.type      = Remove;
    entry.path      = deserializePath( pathStr );
  }
  else if ( line.startsWith( ">> " ) ) {
    QString rest = line.mid( 3 ).trimmed();
    int splitIdx = rest.indexOf( " | " );
    if ( splitIdx != -1 ) {
      QString fromStr = rest.left( splitIdx ).trimmed();
      QString toStr   = rest.mid( splitIdx + 3 ).trimmed();
      entry.type      = MoveFolder;
      entry.path      = deserializePath( fromStr );
      entry.destPath  = deserializePath( toStr );
    }
  }
  else if ( line.startsWith( "> " ) ) {
    QString rest = line.mid( 2 ).trimmed();
    int splitIdx = rest.indexOf( " | " );
    if ( splitIdx != -1 ) {
      QString fromStr = rest.left( splitIdx ).trimmed();
      QString toStr   = rest.mid( splitIdx + 3 ).trimmed();
      entry.type      = Move;
      entry.path      = deserializePath( fromStr );
      entry.destPath  = deserializePath( toStr );
    }
  }
  else {
    qWarning() << "Unknown operation type in WAL:" << line;
  }

  return entry;
}

QString FavoritesWAL::serializePath( const QStringList & path )
{
  QStringList escapedParts;
  for ( const QString & part : path ) {
    QString escaped = part;
    escaped.replace( "\\", "\\\\" ); // Escape backslash first
    escaped.replace( "/", "\\/" );   // Escape forward slash
    escaped.replace( "|", "\\|" );   // Escape pipe
    escapedParts.append( escaped );
  }
  return escapedParts.join( "/" );
}

QStringList FavoritesWAL::deserializePath( const QString & pathStr )
{
  QStringList path;
  QString currentPart;
  bool escaped = false;

  for ( int i = 0; i < pathStr.length(); ++i ) {
    QChar c = pathStr[ i ];
    if ( escaped ) {
      currentPart.append( c );
      escaped = false;
    }
    else if ( c == '\\' ) {
      escaped = true;
    }
    else if ( c == '/' ) {
      path.append( currentPart );
      currentPart.clear();
    }
    else {
      currentPart.append( c );
    }
  }
  path.append( currentPart );
  return path;
}
