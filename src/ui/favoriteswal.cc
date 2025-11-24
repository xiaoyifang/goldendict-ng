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

bool FavoritesWAL::logAdd( const QStringList & path )
{
  QJsonObject entry;
  entry[ "op" ] = "add";
  entry[ "ts" ] = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

bool FavoritesWAL::logRemove( const QStringList & path )
{
  QJsonObject entry;
  entry[ "op" ] = "remove";
  entry[ "ts" ] = QDateTime::currentSecsSinceEpoch();
  entry[ "path" ] = QJsonArray::fromStringList( path );

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

bool FavoritesWAL::logMove( const QStringList & fromPath, const QStringList & toPath )
{
  QJsonObject entry;
  entry[ "op" ] = "move";
  entry[ "ts" ] = QDateTime::currentSecsSinceEpoch();
  entry[ "from" ] = QJsonArray::fromStringList( fromPath );
  entry[ "to" ] = QJsonArray::fromStringList( toPath );

  QJsonDocument doc( entry );
  return appendEntry( doc.toJson( QJsonDocument::Compact ) );
}

QList< QPair< FavoritesWAL::OperationType, QVariant > > FavoritesWAL::replay()
{
  QList< QPair< OperationType, QVariant > > operations;

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

    auto op = parseEntry( line );
    if ( op.first != Add && op.first != Remove && op.first != Move ) {
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

QPair< FavoritesWAL::OperationType, QVariant > FavoritesWAL::parseEntry( const QString & line )
{
  QJsonDocument doc = QJsonDocument::fromJson( line.toUtf8() );
  if ( doc.isNull() || !doc.isObject() ) {
    qWarning() << "Invalid JSON in WAL entry:" << line;
    return qMakePair( Add, QVariant() ); // Return invalid entry
  }

  QJsonObject obj = doc.object();
  QString op = obj[ "op" ].toString();

  if ( op == "add" ) {
    QJsonArray pathArray = obj[ "path" ].toArray();
    QStringList path;
    for ( const auto & item : pathArray ) {
      path.append( item.toString() );
    }
    return qMakePair( Add, QVariant( path ) );
  }
  else if ( op == "remove" ) {
    QJsonArray pathArray = obj[ "path" ].toArray();
    QStringList path;
    for ( const auto & item : pathArray ) {
      path.append( item.toString() );
    }
    return qMakePair( Remove, QVariant( path ) );
  }
  else if ( op == "move" ) {
    QJsonArray fromArray = obj[ "from" ].toArray();
    QJsonArray toArray = obj[ "to" ].toArray();
    
    QStringList fromPath, toPath;
    for ( const auto & item : fromArray ) {
      fromPath.append( item.toString() );
    }
    for ( const auto & item : toArray ) {
      toPath.append( item.toString() );
    }

    QVariantMap moveData;
    moveData[ "from" ] = fromPath;
    moveData[ "to" ] = toPath;
    return qMakePair( Move, QVariant( moveData ) );
  }

  qWarning() << "Unknown operation type in WAL:" << op;
  return qMakePair( Add, QVariant() ); // Return invalid entry
}
