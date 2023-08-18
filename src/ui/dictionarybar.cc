#include "dictionarybar.hh"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QContextMenuEvent>
#include <QProcess>


using std::vector;

DictionaryBar::DictionaryBar( QWidget * parent,
                              Config::Events & events,
                              QString const & _editDictionaryCommand,
                              unsigned short const & maxDictionaryRefsInContextMenu_ ):
  QToolBar( tr( "&Dictionary Bar" ), parent ),
  mutedDictionaries( 0 ),
  configEvents( events ),
  editDictionaryCommand( _editDictionaryCommand ),
  maxDictionaryRefsInContextMenu( maxDictionaryRefsInContextMenu_ )
{
  setObjectName( "dictionaryBar" );

  maxDictionaryRefsAction =
    new QAction( QIcon( ":/icons/expand_opt.png" ), tr( "Extended menu with all dictionaries..." ), this );

  connect( &events, &Config::Events::mutedDictionariesChanged, this, &DictionaryBar::mutedDictionariesChanged );

  connect( this, &QToolBar::actionTriggered, this, &DictionaryBar::actionWasTriggered );
}

static QString elideDictName( QString const & name )
{
  // Some names are way too long -- we insert an ellipsis in the middle of those

  int const maxSize = 33;

  if ( name.size() <= maxSize )
    return name;

  int const pieceSize = maxSize / 2 - 1;

  return name.left( pieceSize ) + QChar( 0x2026 ) + name.right( pieceSize );
}

void DictionaryBar::setDictionaries( vector< sptr< Dictionary::Class > > const & dictionaries )
{
  setUpdatesEnabled( false );

  allDictionaries.clear();
  allDictionaries = dictionaries;

  clear();
  dictActions.clear();

  for ( unsigned x = 0; x < dictionaries.size(); ++x ) {
    QIcon icon = dictionaries[ x ]->getIcon();

    QString dictName = QString::fromUtf8( dictionaries[ x ]->getName().c_str() );

    QAction * action = addAction( icon, elideDictName( dictName ) );

    action->setToolTip( dictName ); // Tooltip need not be shortened

    QString id = QString::fromStdString( dictionaries[ x ]->getId() );

    action->setData( id );

    action->setCheckable( true );

    action->setChecked( mutedDictionaries ? !mutedDictionaries->contains( id ) : true );

    dictActions.append( action );
  }

  setDictionaryIconSize( 21 );

  setUpdatesEnabled( true );
}

void DictionaryBar::setDictionaryIconSize( int extent )
{
  setIconSize( QSize( extent, extent ) );
}

void DictionaryBar::contextMenuEvent( QContextMenuEvent * event )
{
  showContextMenu( event );
}

void DictionaryBar::showContextMenu( QContextMenuEvent * event, bool extended )
{
  QMenu menu( this );

  QAction * editAction = menu.addAction( QIcon( ":/icons/bookcase.svg" ), tr( "Edit this group" ) );

  QAction * infoAction           = NULL;
  QAction * headwordsAction      = NULL;
  QAction * editDictAction       = NULL;
  QAction * openDictFolderAction = NULL;
  QString dictFilename;

  QAction * dictAction = actionAt( event->x(), event->y() );
  if ( dictAction ) {
    Dictionary::Class * pDict = nullptr;
    QString const id          = dictAction->data().toString();
    for ( auto & allDictionarie : allDictionaries ) {
      if ( id.compare( allDictionarie->getId().c_str() ) == 0 ) {
        pDict = allDictionarie.get();
        break;
      }
    }

    if ( pDict ) {
      infoAction = menu.addAction( tr( "Dictionary info" ) );

      if ( pDict->isLocalDictionary() ) {
        if ( pDict->getWordCount() > 0 )
          headwordsAction = menu.addAction( tr( "Dictionary headwords" ) );

        openDictFolderAction = menu.addAction( tr( "Open dictionary folder" ) );

        if ( !editDictionaryCommand.isEmpty() ) {
          if ( !pDict->getMainFilename().isEmpty() ) {
            dictFilename   = pDict->getMainFilename();
            editDictAction = menu.addAction( tr( "Edit dictionary" ) );
          }
        }
      }
    }
  }

  if ( !dictActions.empty() )
    menu.addSeparator();

  unsigned refsAdded = 0;

  for ( auto & dictAction : dictActions ) {

    // Enough! Or the menu would become too large.
    if ( refsAdded++ >= maxDictionaryRefsInContextMenu && !extended ) {
      menu.addSeparator();
      menu.addAction( maxDictionaryRefsAction );
      break;
    }

    // We need new action, since the one we have has text elided
    QAction * action = menu.addAction( dictAction->icon(), dictAction->toolTip() );

    action->setCheckable( true );
    action->setChecked( dictAction->isChecked() );
    action->setData( QVariant::fromValue( (void *)dictAction ) );
    // Force "icon in menu" on all platforms, for
    // usability reasons.
    action->setIconVisibleInMenu( true );
  }

  connect( this, &DictionaryBar::closePopupMenu, &menu, &QWidget::close );

  QAction * result = menu.exec( event->globalPos() );

  if ( result && result == infoAction ) {
    QString const id = dictAction->data().toString();
    emit showDictionaryInfo( id );
    return;
  }

  if ( result && result == headwordsAction ) {
    std::string const id = dictAction->data().toString().toStdString();
    for ( const auto & dict : allDictionaries ) {
      if ( id == dict->getId() ) {
        emit showDictionaryHeadwords( dict.get() );
        break;
      }
    }
    return;
  }

  if ( result && result == openDictFolderAction ) {
    QString const id = dictAction->data().toString();
    emit openDictionaryFolder( id );
    return;
  }

  if ( result && result == editDictAction ) {
    QString command( editDictionaryCommand );
    command.replace( "%GDDICT%", QString( R"("%1")" ).arg( dictFilename ) );
    if ( !QProcess::startDetached( command, QStringList() ) )
      QApplication::beep();
  }

  if ( result && result == maxDictionaryRefsAction ) {
    showContextMenu( event, true );
  }

  if ( result == editAction )
    emit editGroupRequested();
  else if ( result && result->data().value< void * >() )
    ( (QAction *)( result->data().value< void * >() ) )->trigger();

  event->accept();
}

void DictionaryBar::mutedDictionariesChanged()
{
  //GD_DPRINTF( "Muted dictionaries changed\n" );

  if ( !mutedDictionaries )
    return;

  // Update actions

  setUpdatesEnabled( false );

  for ( auto & dictAction : dictActions ) {
    bool const isUnmuted = !mutedDictionaries->contains( dictAction->data().toString() );

    if ( isUnmuted != dictAction->isChecked() )
      dictAction->setChecked( isUnmuted );
  }

  setUpdatesEnabled( true );
}

void DictionaryBar::actionWasTriggered( QAction * action )
{
  if ( !mutedDictionaries )
    return;

  QString const id = action->data().toString();

  if ( id.isEmpty() )
    return; // Some weird action, not our button

  if ( QApplication::keyboardModifiers() & ( Qt::ControlModifier | Qt::ShiftModifier ) ) {
    // Ctrl ,solo mode with single dictionary
    // Shift,toggle back the previous dictionaries

    if ( QApplication::keyboardModifiers() & Qt::ShiftModifier ) {
      if ( !storedMutedSet.isEmpty() ) {
        *mutedDictionaries = storedMutedSet;

        storedMutedSet.clear();
      }
    }
    else {
      // Save dictionaries state
      if ( storedMutedSet.isEmpty() ) {
        storedMutedSet = *mutedDictionaries;
      }

      // Make dictionary solo
      for ( auto & dictAction : dictActions ) {
        QString const dictId = dictAction->data().toString();

        if ( dictId == id )
          mutedDictionaries->remove( dictId );
        else
          mutedDictionaries->insert( dictId );
      }
    }
    configEvents.signalMutedDictionariesChanged();
  }
  else {
    // Normal mode

    storedMutedSet.clear();

    if ( action->isChecked() ) {
      // Unmute the dictionary

      if ( mutedDictionaries->contains( id ) ) {
        mutedDictionaries->remove( id );
        configEvents.signalMutedDictionariesChanged();
      }
    }
    else {
      // Mute the dictionary

      if ( !mutedDictionaries->contains( id ) ) {
        mutedDictionaries->insert( id );
        configEvents.signalMutedDictionariesChanged();
      }
    }
  }
}

void DictionaryBar::dictsPaneClicked( const QString & id )
{
  if ( !isVisible() )
    return;

  for ( auto & dictAction : dictActions ) {
    QString const dictId = dictAction->data().toString();
    if ( dictId == id ) {
      dictAction->activate( QAction::Trigger );
      break;
    }
  }
}
