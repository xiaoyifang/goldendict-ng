/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "history.hh"
#include "config.hh"
#include <QFile>
#include <QSaveFile>
#include <QDebug>

History::History( unsigned size, unsigned maxItemLength_ ):
  maxSize( size ),
  maxItemLength( maxItemLength_ ),
  addingEnabled( true ),
  dirty( false ),
  timerId( 0 )
{
  QFile file( Config::getHistoryFileName() );

  if ( !file.open( QFile::ReadOnly | QIODevice::Text ) )
    return; // No file -- no history

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

History::Item History::getItem( int index )
{
  if ( index < 0 || index >= items.size() ) {
    return {};
  }
  return items.at( index );
}

void History::addItem( Item const & item )
{
  if ( !enabled() )
    return;

  if ( item.word.isEmpty() || item.word.size() > maxItemLength ) {
    // The search looks bogus. Don't save it.
    return;
  }

  //from the normal operation ,there should be only one item in the history at a time.
  if ( items.contains( item ) )
    items.removeOne( item );

  items.push_front( item );

  Item & addedItem = items.first();

  // remove \n and \r to avoid destroying the history file
  addedItem.word.replace( QChar::LineFeed, QChar::Space );
  addedItem.word.replace( QChar::CarriageReturn, QChar::Space );

  ensureSizeConstraints();

  dirty = true;

  emit itemsChanged();
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
  if ( !dirty )
    return true;

  QSaveFile file( Config::getHistoryFileName() );
  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    return false;
  }

  QTextStream out( &file );
  for ( const auto & i : items ) {
    // "0 " is to keep compatibility with the original GD (an unused number)
    out << "0 " << i.word.trimmed() << '\n';
  }

  if ( file.commit() ) {
    dirty = false;
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
