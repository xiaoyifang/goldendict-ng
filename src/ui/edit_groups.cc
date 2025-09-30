/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dict/dictionary.hh"
#include "edit_groups.hh"
#include "instances.hh"
#include <QInputDialog>
#include <QMessageBox>
#include <QToolButton>

using std::vector;

Groups::Groups( QWidget * parent,
                const vector< sptr< Dictionary::Class > > & dicts_,
                const Config::Groups & groups_,
                const Config::Group & order ):
  QWidget( parent ),
  dicts( dicts_ ),
  groups( groups_ )
{
  ui.setupUi( this );

  resetData( dicts_, groups_, order );

  ui.searchLine->applyTo( ui.dictionaries );
  addAction( ui.searchLine->getFocusAction() );
  groupsListMenu = new QMenu( tr( "Group tabs" ), ui.groups );

  groupsListButton = new QToolButton( ui.groups );
  groupsListButton->setAutoRaise( true );
  groupsListButton->setIcon( QIcon( ":/icons/windows-list.svg" ) );
  groupsListButton->setMenu( groupsListMenu );
  groupsListButton->setToolTip( tr( "Open groups list" ) );
  groupsListButton->setPopupMode( QToolButton::InstantPopup );
  ui.groups->setCornerWidget( groupsListButton );
  groupsListButton->setFocusPolicy( Qt::ClickFocus );

  connect( groupsListMenu, &QMenu::aboutToShow, this, &Groups::fillGroupsMenu );
  connect( groupsListMenu, &QMenu::triggered, this, &Groups::switchToGroup );

  connect( ui.addGroup, &QAbstractButton::clicked, this, &Groups::addNew );
  connect( ui.renameGroup, &QAbstractButton::clicked, this, &Groups::renameCurrent );
  connect( ui.removeGroup, &QAbstractButton::clicked, this, &Groups::removeCurrent );
  connect( ui.removeAllGroups, &QAbstractButton::clicked, this, &Groups::removeAll );
  connect( ui.addDictsToGroup, &QAbstractButton::clicked, this, &Groups::addToGroup );
  connect( ui.dictionaries, &QAbstractItemView::doubleClicked, this, &Groups::addToGroup );
  connect( ui.removeDictsFromGroup, &QAbstractButton::clicked, this, &Groups::removeFromGroup );
  connect( ui.groups, &DictGroupsWidget::showDictionaryInfo, this, &Groups::showDictionaryInfo );

  connect( ui.autoGroups, &QAbstractButton::clicked, this, &Groups::addAutoGroups );
  connect( ui.autoGroupsFolders, &QAbstractButton::clicked, this, &Groups::addAutoGroupsByFolders );
  connect( ui.groupMetadata, &QAbstractButton::clicked, this, &Groups::groupsByMetadata );

  ui.dictionaries->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( ui.dictionaries, &QWidget::customContextMenuRequested, this, &Groups::showDictInfo );

  countChanged();
}

void Groups::resetData( const vector< sptr< Dictionary::Class > > & dicts_,
                        const Config::Groups & groups_,
                        const Config::Group & order )
{
  // Populate the dictionaries' list
  ui.dictionaries->setAsSource();
  ui.dictionaries->populate( Instances::Group( order, dicts_, Config::Group() ).dictionaries, dicts_ );

  // Populate groups' widget
  ui.groups->populate( groups_, dicts_, ui.dictionaries->getCurrentDictionaries() );
}

void Groups::editGroup( unsigned id )
{
  for ( int x = 0; x < groups.size(); ++x ) {
    if ( groups[ x ].id == id ) {
      ui.groups->setCurrentIndex( x );
      ui.groups->currentWidget()->setFocus();
      break;
    }
  }
}

void Groups::updateDictionaryOrder( const Config::Group & order )
{
  // Make sure it differs from what we have
  Instances::Group newOrder( order, dicts, Config::Group() );

  if ( ui.dictionaries->getCurrentDictionaries() != newOrder.dictionaries ) {
    // Repopulate
    ui.dictionaries->populate( newOrder.dictionaries, dicts );
  }
}

Config::Groups Groups::getGroups() const
{
  return ui.groups->makeGroups();
}

void Groups::countChanged()
{
  bool en = ui.groups->count();

  ui.renameGroup->setEnabled( en );
  ui.removeGroup->setEnabled( en );
  ui.removeAllGroups->setEnabled( en );

  int stretch = ui.groups->count() / 5;
  if ( stretch > 3 ) {
    stretch = 3;
  }
  ui.gridLayout->setColumnStretch( 2, stretch );
}

void Groups::addNew()
{
  bool ok;

  QString name = QInputDialog::getText( this,
                                        tr( "Add group" ),
                                        tr( "Give a name for the new group:" ),
                                        QLineEdit::Normal,
                                        "",
                                        &ok );

  if ( ok ) {
    ui.groups->addNewGroup( name );
    countChanged();
  }
}

void Groups::addAutoGroups()
{
  ui.groups->addAutoGroups();
  countChanged();
}

void Groups::addAutoGroupsByFolders()
{
  ui.groups->addAutoGroupsByFolders();
  countChanged();
}

void Groups::groupsByMetadata()
{
  ui.groups->groupsByMetadata();
  countChanged();
}
void Groups::renameCurrent()
{
  int current = ui.groups->currentIndex();

  if ( current < 0 ) {
    return;
  }

  bool ok;

  QString name = QInputDialog::getText( this,
                                        tr( "Rename group" ),
                                        tr( "Give a new name for the group:" ),
                                        QLineEdit::Normal,
                                        ui.groups->getCurrentGroupName(),
                                        &ok );

  if ( ok ) {
    ui.groups->renameCurrentGroup( name );
  }
}

void Groups::removeCurrent()
{
  int current = ui.groups->currentIndex();

  if ( current >= 0
       && QMessageBox::question(
            this,
            tr( "Remove group" ),
            tr( "Are you sure you want to remove the group <b>%1</b>?" ).arg( ui.groups->getCurrentGroupName() ),
            QMessageBox::Yes,
            QMessageBox::Cancel )
         == QMessageBox::Yes ) {
    ui.groups->removeCurrentGroup();
    countChanged();
  }
}

void Groups::removeAll()
{
  int current = ui.groups->currentIndex();

  if ( current >= 0
       && QMessageBox::question( this,
                                 tr( "Remove all groups" ),
                                 tr( "Are you sure you want to remove all the groups?" ),
                                 QMessageBox::Yes,
                                 QMessageBox::Cancel )
         == QMessageBox::Yes ) {
    ui.groups->removeAllGroups();
    countChanged();
  }
}

void Groups::addToGroup()
{
  int current = ui.groups->currentIndex();

  if ( current >= 0 ) {
    ui.groups->getCurrentModel()->addSelectedUniqueFromModel( ui.dictionaries->selectionModel() );
  }
}

void Groups::removeFromGroup()
{
  int current = ui.groups->currentIndex();

  if ( current >= 0 ) {
    ui.groups->getCurrentModel()->removeSelectedRows( ui.groups->getCurrentSelectionModel() );
  }
}

void Groups::showDictInfo( const QPoint & pos )
{
  QVariant data =
    ui.dictionaries->getModel()->data( ui.searchLine->mapToSource( ui.dictionaries->indexAt( pos ) ), Qt::EditRole );
  QString id;
  if ( data.canConvert< QString >() ) {
    id = data.toString();
  }

  if ( !id.isEmpty() ) {
    const vector< sptr< Dictionary::Class > > & dicts = ui.dictionaries->getCurrentDictionaries();
    unsigned n;
    for ( n = 0; n < dicts.size(); n++ ) {
      if ( id.compare( QString::fromUtf8( dicts.at( n )->getId().c_str() ) ) == 0 ) {
        break;
      }
    }
    if ( n < dicts.size() ) {
      emit showDictionaryInfo( id );
    }
  }
}

void Groups::fillGroupsMenu()
{
  groupsListMenu->clear();
  for ( int i = 0; i < ui.groups->count(); i++ ) {
    QAction * act = groupsListMenu->addAction( ui.groups->tabText( i ) );
    act->setData( i );
    if ( ui.groups->currentIndex() == i ) {
      QFont f( act->font() );
      f.setBold( true );
      act->setFont( f );
    }
  }

  if ( groupsListMenu->actions().size() > 1 ) {
    groupsListMenu->setActiveAction( groupsListMenu->actions().at( ui.groups->currentIndex() ) );
  }
}

void Groups::switchToGroup( QAction * act )
{
  int idx = act->data().toInt();
  ui.groups->setCurrentIndex( idx );
}
