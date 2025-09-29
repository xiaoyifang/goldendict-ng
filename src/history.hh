/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QObject>
#include <QList>
#include <QString>

constexpr unsigned DEFAULT_MAX_HISTORY_ITEM_LENGTH = 256;


/// Search history
///
/// To work with the original GD's history file,
/// every entry starts with a number like "123 word"
///
class History: public QObject
{
  Q_OBJECT

public:

  /// An item in history
  struct Item
  {
    QString word;

    // For assisting QList::contains & QList::removeOne
    bool operator==( const Item & other ) const
    {
      return QString::compare( word, other.word, Qt::CaseInsensitive ) == 0;
    }
  };

  /// Loads history from its file. If the loading fails, the result would be an empty history.
  explicit History( unsigned size = 256, unsigned maxItemLength = DEFAULT_MAX_HISTORY_ITEM_LENGTH );

  /// Adds new item. The item is always added at the beginning of the list.
  /// If there was such an item already somewhere on the list, it gets removed
  /// from there. If otherwise the resulting list gets too large, the oldest
  /// item gets removed from the end of the list.
  void addItem( const Item & );

  Item getItem( int index );

  /// Remove item with given index from list
  void removeItem( int index )
  {
    // Save to temporary file before removing an item
    if ( index >= 0 && index < items.size() ) {
      saveTemp( items.at( index ), '-' );
    }

    items.removeAt( index );
    dirty = true;
    emit itemsChanged();
  }

  /// Attempts saving history. Returns true if succeeded - false otherwise.
  /// Since history isn't really that valuable, failures can be ignored.
  bool save();

  /// Clears history.
  void clear();

  /// History size.
  int size() const;

  /// Gets the current items. The first one is the newest one on the list.
  const QList< Item > & getItems() const
  {
    return items;
  }

  /// Enable/disable add words to hystory
  void enableAdd( bool enable )
  {
    addingEnabled = enable;
  }
  bool enabled()
  {
    return addingEnabled;
  }

  void setMaxSize( unsigned maxSize_ );

  void setSaveInterval( unsigned interval );

  unsigned getMaxSize()
  {
    return maxSize;
  }

  unsigned getMaxItemLength() const
  {
    return maxItemLength;
  }

signals:

  /// Signals the changes in items in response to addItem() or clear().
  void itemsChanged();

private:

  /// Returns true if the items list has been modified
  /// in order to fit into the constraints.
  bool ensureSizeConstraints();

  /// Save history to temporary file for crash recovery
  void saveTemp( const Item & item, QChar operation );

  /// Load history from temporary file if main file is corrupted or missing
  void loadTemp();

  /// Remove temporary file
  void removeTemp();

  QList< Item > items;
  unsigned maxSize;
  unsigned maxItemLength;
  bool addingEnabled;
  bool dirty;
  int timerId;

protected:
  virtual void timerEvent( QTimerEvent * );
};
