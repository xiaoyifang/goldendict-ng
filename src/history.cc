/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "history.hh"
#include "config.hh"
#include <QFile>
#include <QSaveFile>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

// Helper function to get the temporary history file name
QString getTempHistoryFileName()
{
  return Config::getHomeDir().filePath( "history.tmp" );
}

History::History( unsigned size, unsigned maxItemLength_ ):
  maxSize( size ),
  maxItemLength( maxItemLength_ ),
  addingEnabled( true ),
  dirty( false ),
  timerId( 0 )
{
  // First try to load from main history file
  QFile file( Config::getHistoryFileName() );

  if ( file.open( QFile::ReadOnly | QIODevice::Text ) ) {
    QTextStream in( &file );
    while ( !in.atEnd() && items.size() <= maxSize ) {
      QString line = in.readLine( 4096 );

      auto firstSpace = line.indexOf( ' ' );

      if ( firstSpace < 0 || firstSpace == line.size() ) {
        break;
      }

      QString t = line.right( line.size() - firstSpace - 1 ).trimmed();

      if ( !t.isEmpty() ) {
        items.push_back( Item{ line.right( line.size() - firstSpace - 1 ).trimmed() } );
      }
    }
  }

  // Then try to load from temporary file (in case of previous crash)
  // This will add the most recent item at the beginning if it exists
  loadTemp();

  // Remove temporary file after successful load
  removeTemp();
}

History::Item History::getItem( int index )
{
  if ( index < 0 || index >= items.size() ) {
    return {};
  }
  return items.at( index );
}

void History::addItem( const Item & item )
{
  if ( !enabled() ) {
    return;
  }

  if ( item.word.isEmpty() || item.word.size() > maxItemLength ) {
    // The search looks bogus. Don't save it.
    return;
  }

  //from the normal operation ,there should be only one item in the history at a time.
  if ( items.contains( item ) ) {
    items.removeOne( item );
  }

  items.push_front( item );

  Item & addedItem = items.first();

  // remove \n and \r to avoid destroying the history file
  addedItem.word.replace( QChar::LineFeed, QChar::Space );
  addedItem.word.replace( QChar::CarriageReturn, QChar::Space );

  ensureSizeConstraints();

  dirty = true;

  emit itemsChanged();

  // Save to temporary file immediately after adding an item
  saveTemp( addedItem, '+' );
}

bool History::ensureSizeConstraints()
{
  bool changed = false;
  while ( items.size() > (int)maxSize ) {
    items.pop_back();
    changed = true;
    dirty   = true;
  }

  return changed;
}

void History::setMaxSize( unsigned maxSize_ )
{
  maxSize = maxSize_;
  if ( ensureSizeConstraints() ) {
    emit itemsChanged();
  }
}

int History::size() const
{
  return items.size();
}

bool History::save()
{
  if ( !dirty ) {
    return true;
  }

  QSaveFile file( Config::getHistoryFileName() );
  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    return false;
  }

  QTextStream out( &file );
  for ( const auto & i : std::as_const( items ) ) {
    // "0 " is to keep compatibility with the original GD (an unused number)
    out << "0 " << i.word.trimmed() << '\n';
  }

  if ( file.commit() ) {
    dirty = false;
    // Remove temporary file after successful save to main file
    removeTemp();
    return true;
  }

  qDebug() << "Failed to save history file";
  return false;
}

void History::clear()
{
  items.clear();
  dirty = true;

  emit itemsChanged();

  // Remove temporary file when clearing history
  removeTemp();
}

void History::setSaveInterval( unsigned interval )
{
  if ( timerId != 0 ) {
    killTimer( timerId );
    timerId = 0;
  }
  if ( interval != 0 ) {
    if ( dirty ) {
      save();
    }
    timerId = startTimer( interval * 60000 );
  }
}

void History::timerEvent( QTimerEvent * ev )
{
  Q_UNUSED( ev );
  save();
}

void History::saveTemp( const Item & item, QChar operation )
{
  QSaveFile file( getTempHistoryFileName() );
  // Use Append mode to accumulate new records in temporary file
  if ( !file.open( QFile::Append | QIODevice::Text ) ) {
    return; // Failed to open temporary file
  }

  QTextStream out( &file );
  // operation indicates the type of operation: '+' for add, '-' for remove
  out << operation << " " << item.word.trimmed() << '\n';

  if ( file.commit() ) {
    // Successfully saved to temporary file
    // Note: We don't reset dirty flag here as the main file is not yet saved
    return;
  }

  qDebug() << "Failed to save temporary history file";
}

void History::loadTemp()
{
  QFile file( getTempHistoryFileName() );

  if ( !file.open( QFile::ReadOnly | QIODevice::Text ) ) {
    return; // No temporary file -- no history to recover
  }

  QTextStream in( &file );
  while ( !in.atEnd() && items.size()  <= maxSize ) {
    QString line = in.readLine( 4096 );

    if ( line.isEmpty() ) {
      continue;
    }

    // First character indicates the operation: '+' for add, '-' for remove
    QChar operation = line[ 0 ];
    QString word;

    if ( line.size() > 2 ) {
      word = line.mid( 2 ).trimmed(); // Skip the operation character and space
    }

    if ( word.isEmpty() ) {
      continue;
    }

    Item newItem{ word };

    if ( operation == '+' ) {
      // Check if the item already exists in the main history or in items to remove
      if ( !items.contains( newItem ) ) {
        // Add to temporary list (they are in chronological order)
        items.push_front( newItem );
      }
    }
    else if ( operation == '-' ) {
      items.removeOne(newItem);
    }
  }

  qDebug() << "Recovered items from temporary file";
}

void History::removeTemp()
{
  QFile::remove( getTempHistoryFileName() );
}
