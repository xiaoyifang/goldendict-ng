/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "groupcombobox.hh"
#include <QEvent>
#include <QShortcutEvent>

GroupComboBox::GroupComboBox( QWidget * parent ):
  QComboBox( parent ),
  popupAction( this ),
  selectNextAction( this ),
  selectPreviousAction( this )
{
  setIconSize( QSize( 16, 16 ) );
  setSizeAdjustPolicy( AdjustToContents );
  setToolTip( tr( "Choose a Group (Alt+G)" ) );

  popupAction.setShortcut( QKeySequence( "Alt+G" ) );
  connect( &popupAction, &QAction::triggered, this, &GroupComboBox::popupGroups );

  addAction( &popupAction );

  selectNextAction.setShortcut( QKeySequence( "Alt+PgDown" ) );
  connect( &selectNextAction, &QAction::triggered, this, &GroupComboBox::selectNextGroup );
  addAction( &selectNextAction );

  selectPreviousAction.setShortcut( QKeySequence( "Alt+PgUp" ) );
  connect( &selectPreviousAction, &QAction::triggered, this, &GroupComboBox::selectPreviousGroup );
  addAction( &selectPreviousAction );

  setMaxVisibleItems( 30 );
}

void GroupComboBox::fill( const Instances::Groups & groups )
{
  unsigned prevId = 0;

  if ( count() ) {
    prevId = itemData( currentIndex() ).toUInt();
  }

  clear();

  for ( QMap< int, int >::iterator i = shortcuts.begin(); i != shortcuts.end(); ++i ) {
    releaseShortcut( i.key() );
  }

  shortcuts.clear();

  for ( unsigned x = 0; x < groups.size(); ++x ) {
    addItem( groups[ x ].makeIcon(), groups[ x ].name, groups[ x ].id );

    if ( prevId == groups[ x ].id ) {
      setCurrentIndex( x );
    }

    // Create a shortcut

    if ( !groups[ x ].shortcut.isEmpty() ) {
      int id = grabShortcut( groups[ x ].shortcut );
      setShortcutEnabled( id );

      shortcuts.insert( id, x );
    }
  }
  updateGeometry();
}

QSize GroupComboBox::sizeHint() const
{
  QSize s = QComboBox::sizeHint();
  if ( count() > 0 ) {
    QFontMetrics fm( font() );
    int maxW = 0;
    for ( int i = 0; i < count(); ++i ) {
      maxW = qMax( maxW, fm.horizontalAdvance( itemText( i ) ) );
    }

    int iconW = iconSize().width();
    if ( iconW > 0 ) {
      iconW += 6; // Icon plus some margin
    }

    // 35 pixels for the arrow and internal padding
    int estimated = iconW + maxW + 35;
    if ( s.width() < estimated ) {
      s.setWidth( estimated );
    }
  }
  return s;
}

bool GroupComboBox::event( QEvent * event )
{
  if ( event->type() == QEvent::Shortcut ) {
    QShortcutEvent * ev = (QShortcutEvent *)event;

    QMap< int, int >::const_iterator i = shortcuts.find( ev->shortcutId() );

    if ( i != shortcuts.end() ) {
      ev->accept();
      setCurrentIndex( i.value() );
      return true;
    }
  }

  return QComboBox::event( event );
}

QList< QAction * > GroupComboBox::getExternActions()
{
  QList< QAction * > list;
  list.append( &popupAction );
  list.append( &selectNextAction );
  list.append( &selectPreviousAction );
  return list;
}

void GroupComboBox::setCurrentGroup( unsigned id )
{
  for ( int x = 0; x < count(); ++x ) {
    if ( itemData( x ).toUInt() == id ) {
      setCurrentIndex( x );
      break;
    }
  }
}

unsigned GroupComboBox::getCurrentGroup() const
{
  if ( !count() ) {
    return 0;
  }

  return itemData( currentIndex() ).toUInt();
}

void GroupComboBox::popupGroups()
{
  showPopup();
}

void GroupComboBox::selectNextGroup()
{
  if ( count() <= 1 ) {
    return;
  }
  int n = currentIndex() + 1;
  if ( n >= count() ) {
    n = 0;
  }
  setCurrentIndex( n );
}

void GroupComboBox::selectPreviousGroup()
{
  if ( count() <= 1 ) {
    return;
  }
  int n = currentIndex() - 1;
  if ( n < 0 ) {
    n = count() - 1;
  }
  setCurrentIndex( n );
}
