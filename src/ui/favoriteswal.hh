/* This file is (c) 2024 xiaoyifang
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>

/// Write-Ahead Log manager for favorites operations
/// Ensures no data loss by logging all operations before they are applied
class FavoritesWAL: public QObject
{
  Q_OBJECT

public:
  enum OperationType {
    Add,
    Remove,
    Move
  };

  explicit FavoritesWAL( const QString & walFilename, QObject * parent = nullptr );
  ~FavoritesWAL();

  /// Log an add operation to the WAL
  bool logAdd( const QStringList & path, bool isFolder = false );

  /// Log a remove operation to the WAL
  bool logRemove( const QStringList & path, bool isFolder = false );

  /// Log a move operation to the WAL
  bool logMove( const QStringList & fromPath, const QStringList & toPath, bool isFolder = false );

  /// Replay all operations from the WAL
  /// Returns list of operations to be applied
  QList< QPair< OperationType, QVariantMap > > replay();

  /// Clear the WAL file (called after successful compaction)
  bool clear();

  /// Check if WAL file exists and has content
  bool hasEntries() const;

private:
  QString m_walFilename;
  QFile m_walFile;

  /// Append a JSON line to the WAL
  bool appendEntry( const QByteArray & jsonLine );

  /// Parse a single WAL entry
  QPair< OperationType, QVariant > parseEntry( const QString & line );
};
