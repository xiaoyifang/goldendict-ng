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
  QJsonObject entry;
  // 使用不同的操作类型字符串来区分普通添加和文件夹添加
  entry[ "op" ]   = isFolder ? "addfolder" : "add";
  entry[ "ts" ]   = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );
  
  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

bool FavoritesWAL::logRemove( const QStringList & path, bool isFolder )
{
  QJsonObject entry;
  // 使用不同的操作类型字符串来区分普通删除和文件夹删除
  entry[ "op" ]   = isFolder ? "removefolder" : "remove";
  entry[ "ts" ]   = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );
  
  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

bool FavoritesWAL::logMove( const QStringList & fromPath, const QStringList & toPath, bool isFolder )
{
  QJsonObject entry;
  entry[ "op" ]       = "move";
  entry[ "ts" ]       = QDateTime::currentSecsSinceEpoch();
  entry[ "from" ]     = QJsonArray::fromStringList( fromPath );
  entry[ "to" ]       = QJsonArray::fromStringList( toPath );
  // Move操作仍然需要isFolder字段，因为它没有单独的文件夹版本
  entry[ "isFolder" ] = isFolder;

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

QList< QPair< FavoritesWAL::OperationType, QVariantMap > > FavoritesWAL::replay()
{
  QList< QPair< FavoritesWAL::OperationType, QVariantMap > > operations;

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

    QPair< FavoritesWAL::OperationType, QVariantMap > op = parseEntry( line );
    if ( op.first != Add && op.first != Remove && op.first != Move && op.first != AddFolder && op.first != RemoveFolder ) {
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

bool FavoritesWAL::appendEntry( const QByteArray & jsonLine )
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
  }
  file.write( jsonLine );
  file.write( "\n" );

  if ( !file.commit() ) {
    qWarning() << "Failed to commit WAL entry:" << file.errorString();
    return false;
  }

  return true;
}

QPair< FavoritesWAL::OperationType, QVariantMap > FavoritesWAL::parseEntry( const QString & line )
{
  QJsonDocument doc = QJsonDocument::fromJson( line.toUtf8() );
  if ( doc.isNull() || !doc.isObject() ) {
    qWarning() << "Invalid JSON in WAL entry:" << line;
    return qMakePair( Add, QVariantMap() ); // Return invalid entry
  }

  QJsonObject obj = doc.object();
  QString op      = obj[ "op" ].toString();
  // 不再从WAL条目中读取isFolder字段，因为我们已经通过操作类型区分了文件夹操作

  if ( op == "add" || op == "addfolder" ) {
    QJsonArray pathArray = obj[ "path" ].toArray();
    QStringList path;
    for ( const auto & item : pathArray ) {
      path.append( item.toString() );
    }
    QVariantMap data;
    data[ "path" ] = path;
    // 根据操作类型字符串区分普通添加和文件夹添加
    return qMakePair( (op == "addfolder") ? AddFolder : Add, data );
  }
  else if ( op == "remove" || op == "removefolder" ) {
    QJsonArray pathArray = obj[ "path" ].toArray();
    QStringList path;
    for ( const auto & item : pathArray ) {
      path.append( item.toString() );
    }
    QVariantMap data;
    data[ "path" ] = path;
    // 根据操作类型字符串区分普通删除和文件夹删除
    return qMakePair( (op == "removefolder") ? RemoveFolder : Remove, data );
  }
  else if ( op == "move" ) {
    QJsonArray fromArray = obj[ "from" ].toArray();
    QJsonArray toArray   = obj[ "to" ].toArray();

    QStringList fromPath, toPath;
    for ( const auto & item : fromArray ) {
      fromPath.append( item.toString() );
    }
    for ( const auto & item : toArray ) {
      toPath.append( item.toString() );
    }

    QVariantMap moveData;
    moveData[ "from" ]     = fromPath;
    moveData[ "to" ]       = toPath;
    moveData[ "isFolder" ] = isFolder;
    return qMakePair( Move, moveData );
  }

  qWarning() << "Unknown operation type in WAL:" << op;
  return qMakePair( Add, QVariantMap() ); // Return invalid entry
}
