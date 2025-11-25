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
    Move,
    AddFolder,
    RemoveFolder,
    MoveFolder
  };

  struct Entry {
    OperationType type;
    QStringList path;      // Used for Add/Remove, and as Source for Move
    QStringList destPath;  // Used for Move (Destination)

    bool isFolder() const {
      return type == AddFolder || type == RemoveFolder || type == MoveFolder;
    }
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
  QList< Entry > replay();

  /// Clear the WAL file (called after successful compaction)
  bool clear();

  /// Check if WAL file exists and has content
  bool hasEntries() const;

private:
  QString m_walFilename;
  QFile m_walFile;

  /// Append a text line to the WAL
  bool appendEntry( const QString & line );

  /// Parse a single WAL entry
  Entry parseEntry( const QString & line );

  /// Serialize path to string with escaping
  static QString serializePath( const QStringList & path );

  /// Deserialize path from string with unescaping
  static QStringList deserializePath( const QString & pathStr );
};
