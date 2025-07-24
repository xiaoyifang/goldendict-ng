#include "dictionarybar.hh"
#include "instances.hh"
#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QContextMenuEvent>
#include <QStyle>


using std::vector;

DictionaryBar::DictionaryBar( QWidget * parent,
                              Config::Events & events,
                              const unsigned short & maxDictionaryRefsInContextMenu_ ):
  QToolBar( tr( "&Dictionary Bar" ), parent ),
  mutedDictionaries( nullptr ),
  configEvents( events ),
  maxDictionaryRefsInContextMenu( maxDictionaryRefsInContextMenu_ )
{
  normalIconSize = { this->iconSize().height(), this->iconSize().height() };

  setObjectName( "dictionaryBar" );

  maxDictionaryRefsAction =
    new QAction( QIcon( ":/icons/expand_opt.png" ), tr( "Extended menu with all dictionaries..." ), this );

  connect( &events, &Config::Events::mutedDictionariesChanged, this, &DictionaryBar::mutedDictionariesChanged );

  connect( this, &QToolBar::actionTriggered, this, &DictionaryBar::actionWasTriggered );
}

static QString elideDictName( const QString & name )
{
  // Some names are way too long -- we insert an ellipsis in the middle of those

  const int maxSize = 33;

  if ( name.size() <= maxSize ) {
    return name;
  }

  const int pieceSize = maxSize / 2 - 1;

  return name.left( pieceSize ) + QChar( 0x2026 ) + name.right( pieceSize );
}

void DictionaryBar::setDictionaries( const vector< sptr< Dictionary::Class > > & dictionaries )
{
  setUpdatesEnabled( false );

  allDictionaries.clear();
  allDictionaries = dictionaries;

  clear();
  dictActions.clear();

  for ( const auto & dictionary : dictionaries ) {
    QIcon icon = dictionary->getIcon();

    QString dictName = QString::fromUtf8( dictionary->getName().c_str() );

    QAction * action = addAction( icon, elideDictName( dictName ) );

    action->setToolTip( dictName ); // Tooltip need not be shortened

    QString id = QString::fromStdString( dictionary->getId() );

    action->setData( id );

    action->setCheckable( true );

    action->setChecked( mutedDictionaries ? !mutedDictionaries->contains( id ) : true );

    dictActions.append( action );
  }


  setUpdatesEnabled( true );
}

void DictionaryBar::updateToGroup( const Instances::Group * grp,
                                   Config::MutedDictionaries * allGroupMutedDictionaries,
                                   Config::Class & cfg )
{
  Q_ASSERT( grp != nullptr ); // should never occur
  if ( !this->toggleViewAction()->isChecked() ) {
    return; // It's not enabled, therefore hidden -- don't waste time
  }

  setMutedDictionaries( nullptr );
  if ( grp->id == GroupId::AllGroupId ) {
    setMutedDictionaries( allGroupMutedDictionaries );
  }
  else {
    Config::Group * _grp = cfg.getGroup( grp->id );
    setMutedDictionaries( _grp ? &_grp->mutedDictionaries : nullptr );
  }
  setDictionaries( grp->dictionaries );
}

void DictionaryBar::setDictionaryIconSize( IconSize size )
{
  switch ( size ) {
    case IconSize::Small: {
      auto smallSize = QApplication::style()->pixelMetric( QStyle::PM_SmallIconSize );
      setIconSize( { smallSize, smallSize } );
      break;
    }

    case IconSize::Normal: {
      setIconSize( normalIconSize );
      break;
    }

    case IconSize::Large: {
      auto largeSize = QApplication::style()->pixelMetric( QStyle::PM_LargeIconSize );
      setIconSize( { largeSize + 10, largeSize + 10 } ); // the value isn't large enough, so we add 10
      break;
    }
  }
}

void DictionaryBar::contextMenuEvent( QContextMenuEvent * event )
{
  showContextMenu( event );
}


void DictionaryBar::showContextMenu( QContextMenuEvent * event, bool extended )
{
  QMenu menu( this );

  const QAction * restoreSelectionAction = nullptr;
  if ( tempSelectionCapturedMuted.has_value() ) {
    restoreSelectionAction = menu.addAction( tr( "Restore selection" ) );
  }

  const QAction * editAction = menu.addAction( QIcon( ":/icons/bookcase.svg" ), tr( "Edit this group" ) );

  const QAction * infoAction           = nullptr;
  const QAction * headwordsAction      = nullptr;
  const QAction * openDictFolderAction = nullptr;


  QString dictFilename;


  const QAction * dictAction = actionAt( event->x(), event->y() );
  if ( dictAction ) {
    Dictionary::Class * pDict = nullptr;
    const QString id          = dictAction->data().toString();
    for ( const auto & dictionary : allDictionaries ) {
      if ( id.compare( dictionary->getId().c_str() ) == 0 ) {
        pDict = dictionary.get();
        break;
      }
    }

    if ( pDict ) {
      infoAction = menu.addAction( tr( "Dictionary info" ) );

      if ( pDict->isLocalDictionary() ) {
        if ( pDict->getWordCount() > 0 ) {
          headwordsAction = menu.addAction( tr( "Dictionary headwords" ) );
        }

        openDictFolderAction = menu.addAction( tr( "Open dictionary folder" ) );
      }
    }
  }

  if ( !dictActions.empty() ) {
    menu.addSeparator();
  }

  unsigned refsAdded = 0;

  for ( const auto & dictAction : std::as_const( dictActions ) ) {

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

  const QAction * result = menu.exec( event->globalPos() );

  if ( result && result == infoAction ) {
    const QString id = dictAction->data().toString();
    emit showDictionaryInfo( id );
    return;
  }

  if ( result && result == headwordsAction ) {
    const std::string id = dictAction->data().toString().toStdString();
    for ( const auto & dict : allDictionaries ) {
      if ( id == dict->getId() ) {
        emit showDictionaryHeadwords( dict.get() );
        break;
      }
    }
    return;
  }

  if ( result && result == openDictFolderAction ) {
    const QString id = dictAction->data().toString();
    emit openDictionaryFolder( id );
    return;
  }

  if ( result && result == maxDictionaryRefsAction ) {
    showContextMenu( event, true );
  }

  if ( result && result == restoreSelectionAction ) {
    *mutedDictionaries = tempSelectionCapturedMuted.value();
    tempSelectionCapturedMuted.reset();
    configEvents.signalMutedDictionariesChanged();
  }

  if ( result == editAction ) {
    emit editGroupRequested();
  }
  else if ( result && result->data().value< void * >() ) {
    ( (QAction *)( result->data().value< void * >() ) )->trigger();
  }

  event->accept();
}

void DictionaryBar::mutedDictionariesChanged()
{
  //qDebug( "Muted dictionaries changed" );

  if ( !mutedDictionaries ) {
    return;
  }

  // Update actions

  setUpdatesEnabled( false );

  for ( const auto & dictAction : std::as_const( dictActions ) ) {
    const bool isUnmuted = !mutedDictionaries->contains( dictAction->data().toString() );

    if ( isUnmuted != dictAction->isChecked() ) {
      dictAction->setChecked( isUnmuted );
    }
  }

  setUpdatesEnabled( true );
}

void DictionaryBar::actionWasTriggered( QAction * action )
{
  if ( !mutedDictionaries ) {
    return;
  }

  const QString id = action->data().toString();

  if ( id.isEmpty() ) {
    return; // Some weird action, not our button
  }

  //  Ctrl Click Single Selection
  if ( QApplication::keyboardModifiers().testFlag( Qt::ControlModifier ) ) {
    // Ctrl+Clicked the only one selected
    if ( ( dictActions.size() - mutedDictionaries->size() ) == 1 && !action->isChecked() ) {
      mutedDictionaries->clear();
    }
    else {
      selectSingleDict( id );
    }
  }
  ///  Shift Click Capturing
  else if ( QApplication::keyboardModifiers().testFlag( Qt::ShiftModifier ) ) {
    tempSelectionCapturedMuted.emplace( *mutedDictionaries );
    selectSingleDict( id ); // Give user feedback that capturing success by select the one clicked
  }
  else { // Normal clicking
    if ( action->isChecked() ) {
      mutedDictionaries->remove( id );
    }
    else {
      mutedDictionaries->insert( id );
    }
  }
  configEvents.signalMutedDictionariesChanged();
}

void DictionaryBar::selectSingleDict( const QString & id )
{
  for ( auto & dictAction : std::as_const( dictActions ) ) {
    const QString dictId = dictAction->data().toString();
    if ( dictId == id ) {
      mutedDictionaries->remove( dictId );
    }
    else {
      mutedDictionaries->insert( dictId );
    }
  }
}

void DictionaryBar::dictsPaneClicked( const QString & id )
{
  if ( !isVisible() ) {
    return;
  }

  for ( const auto & dictAction : std::as_const( dictActions ) ) {
    const QString dictId = dictAction->data().toString();
    if ( dictId == id ) {
      dictAction->activate( QAction::Trigger );
      break;
    }
  }
}
