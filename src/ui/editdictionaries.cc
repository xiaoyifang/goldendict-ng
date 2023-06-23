/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "editdictionaries.hh"
#include "dict/loaddictionaries.hh"
#include "help.hh"
#include <QTabWidget>
#include <QMessageBox>

using std::vector;

EditDictionaries::EditDictionaries( QWidget * parent,
                                    Config::Class & cfg_,
                                    vector< sptr< Dictionary::Class > > & dictionaries_,
                                    Instances::Groups & groupInstances_,
                                    QNetworkAccessManager & dictNetMgr_ ):
  QDialog( parent, Qt::WindowSystemMenuHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint ),
  cfg( cfg_ ),
  dictionaries( dictionaries_ ),
  groupInstances( groupInstances_ ),
  dictNetMgr( dictNetMgr_ ),
  origCfg( cfg ),
  sources( this, cfg ),
  orderAndProps( new OrderAndProps( this, cfg.dictionaryOrder, cfg.inactiveDictionaries, dictionaries ) ),
  groups( new Groups( this, dictionaries, cfg.groups, orderAndProps->getCurrentDictionaryOrder() ) ),
  dictionariesChanged( false ),
  groupsChanged( false ),
  helpAction( this )
{
  // Some groups may have contained links to non-existnent dictionaries. We
  // would like to preserve them if no edits were done. To that end, we save
  // the initial group readings so that if no edits were really done, we won't
  // be changing groups.
  origCfg.groups = groups->getGroups();
  origCfg.dictionaryOrder = orderAndProps->getCurrentDictionaryOrder();
  origCfg.inactiveDictionaries = orderAndProps->getCurrentInactiveDictionaries();

  ui.setupUi( this );

  setWindowIcon( QIcon(":/icons/dictionary.svg") );

  ui.tabs->clear();

  ui.tabs->addTab( &sources, QIcon(":/icons/sources.png"), tr( "&Sources" ) );
  ui.tabs->addTab( orderAndProps.get(), QIcon( ":/icons/book.svg" ), tr( "&Dictionaries" ) );
  ui.tabs->addTab( groups.get(), QIcon(":/icons/bookcase.svg"), tr( "&Groups" ) );

  connect( ui.buttons, &QDialogButtonBox::clicked, this, &EditDictionaries::buttonBoxClicked );

  connect( &sources, &Sources::rescan, this, &EditDictionaries::rescanSources );

  connect( groups, &Groups::showDictionaryInfo, this, &EditDictionaries::showDictionaryInfo );

  connect( orderAndProps, &OrderAndProps::showDictionaryHeadwords, this, &EditDictionaries::showDictionaryHeadwords );

  helpAction.setShortcut( QKeySequence( "F1" ) );
  helpAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );

  connect( &helpAction, &QAction::triggered, [ this ]() {
    if ( ui.tabs->currentWidget() == this->groups.get() ) {
      Help::openHelpWebpage( Help::section::manage_groups );
    }
    else {
      Help::openHelpWebpage( Help::section::manage_sources );
    }
  } );
  connect( ui.buttons, &QDialogButtonBox::helpRequested, &helpAction, &QAction::trigger );

  addAction( &helpAction );

  connect( ui.tabs, &QTabWidget::currentChanged, this, &EditDictionaries::currentChanged );
}

void EditDictionaries::editGroup( unsigned id )
{
  ui.tabs->setTabVisible( 0, false );

  if ( id == Instances::Group::AllGroupId )
  {
    ui.tabs->setCurrentIndex( 1 );
  }
  else
  {
    ui.tabs->setCurrentIndex( 2 );
    groups->editGroup( id );
  }
}

void EditDictionaries::save( bool rebuildGroups )
{
  const Config::Groups newGroups  = groups->getGroups();
  const Config::Group newOrder    = orderAndProps->getCurrentDictionaryOrder();
  const Config::Group newInactive = orderAndProps->getCurrentInactiveDictionaries();

  if ( isSourcesChanged() )
    acceptChangedSources( rebuildGroups );

  if ( origCfg.groups != newGroups || origCfg.dictionaryOrder != newOrder
       || origCfg.inactiveDictionaries != newInactive ) {
    groupsChanged            = true;
    cfg.groups               = newGroups;
    cfg.dictionaryOrder      = newOrder;
    cfg.inactiveDictionaries = newInactive;
  }
}

void EditDictionaries::accept()
{
  save();
  QDialog::accept();
}

void EditDictionaries::currentChanged( int index )
{
  if ( index == -1 || !isVisible() )
    return; // Sent upon the construction/destruction
  qDebug() << ui.tabs->currentWidget()->objectName();
  if ( lastTabName.isEmpty() || lastTabName == "Sources" ) {
    // We're switching away from the Sources tab -- if its contents were
    // changed, we need to either apply or reject now.

    if ( isSourcesChanged() ) {
      QMessageBox question( QMessageBox::Question,
                            tr( "Sources changed" ),
                            tr( "Some sources were changed. Would you like to accept the changes?" ),
                            QMessageBox::NoButton,
                            this );

      const QPushButton * accept = question.addButton( tr( "Accept" ), QMessageBox::AcceptRole );

      question.addButton( tr( "Cancel" ), QMessageBox::RejectRole );

      question.exec();

      //When accept the changes ,the second and third tab will be recreated. which means the current Index tab will be changed.
      if ( question.clickedButton() == accept ) {
        disconnect( ui.tabs, &QTabWidget::currentChanged, this, &EditDictionaries::currentChanged );

        acceptChangedSources( true );
        ui.tabs->setCurrentIndex( index );
        connect( ui.tabs, &QTabWidget::currentChanged, this, &EditDictionaries::currentChanged );
      }
      else {
        // Prevent tab from switching
        //        return;
      }
    }
  }
  else if ( lastTabName == "OrderAndProps" ) {
    // When switching from the dictionary order, we need to propagate any
    // changes to the groups.
    groups->updateDictionaryOrder( orderAndProps->getCurrentDictionaryOrder() );
  }

  lastTabName = ui.tabs->currentWidget()->objectName();
}

void EditDictionaries::rescanSources()
{
  acceptChangedSources( true );
}

void EditDictionaries::buttonBoxClicked( QAbstractButton * button )
{
  if (ui.buttons->buttonRole(button) == QDialogButtonBox::ApplyRole) {
    save( true );
  }
}

bool EditDictionaries::isSourcesChanged() const
{
  return sources.getPaths() != cfg.paths ||
         sources.getSoundDirs() != cfg.soundDirs ||
         sources.getHunspell() != cfg.hunspell ||
         sources.getTransliteration() != cfg.transliteration ||
         sources.getLingua() != cfg.lingua ||
         sources.getForvo() != cfg.forvo ||
         sources.getMediaWikis() != cfg.mediawikis ||
         sources.getWebSites() != cfg.webSites ||
         sources.getDictServers() != cfg.dictServers ||
         sources.getPrograms() != cfg.programs ||
         sources.getVoiceEngines() != cfg.voiceEngines;
}

void EditDictionaries::acceptChangedSources( bool rebuildGroups )
{
  dictionariesChanged = true;

  Config::Groups savedGroups  = groups->getGroups();
  Config::Group savedOrder    = orderAndProps->getCurrentDictionaryOrder();
  Config::Group savedInactive = orderAndProps->getCurrentInactiveDictionaries();

  cfg.paths           = sources.getPaths();
  cfg.soundDirs       = sources.getSoundDirs();
  cfg.hunspell        = sources.getHunspell();
  cfg.transliteration = sources.getTransliteration();
  cfg.lingua          = sources.getLingua();
  cfg.forvo           = sources.getForvo();
  cfg.mediawikis      = sources.getMediaWikis();
  cfg.webSites        = sources.getWebSites();
  cfg.dictServers     = sources.getDictServers();
  cfg.programs        = sources.getPrograms();
  cfg.voiceEngines    = sources.getVoiceEngines();

  ui.tabs->setUpdatesEnabled( false );
  // Those hold pointers to dictionaries, we need to free them.
  groupInstances.clear();

  groups.clear();
  orderAndProps.clear();

  loadDictionaries( this, true, cfg, dictionaries, dictNetMgr );

  // If no changes to groups were made, update the original data
  const bool noGroupEdits = ( origCfg.groups == savedGroups );

  if ( noGroupEdits )
    savedGroups = cfg.groups;

  Instances::updateNames( savedGroups, dictionaries );

  const bool noOrderEdits = ( origCfg.dictionaryOrder == savedOrder );

  if ( noOrderEdits )
    savedOrder = cfg.dictionaryOrder;

  Instances::updateNames( savedOrder, dictionaries );

  const bool noInactiveEdits = ( origCfg.inactiveDictionaries == savedInactive );

  if ( noInactiveEdits )
    savedInactive  = cfg.inactiveDictionaries;

  Instances::updateNames( savedInactive, dictionaries );

  if ( rebuildGroups ) {
    ui.tabs->removeTab( 1 );
    ui.tabs->removeTab( 1 );

    orderAndProps = new OrderAndProps( this, savedOrder, savedInactive, dictionaries );
    groups        = new Groups( this, dictionaries, savedGroups, orderAndProps->getCurrentDictionaryOrder() );

    ui.tabs->insertTab( 1, orderAndProps.get(), QIcon( ":/icons/book.svg" ), tr( "&Dictionaries" ) );
    ui.tabs->insertTab( 2, groups.get(), QIcon( ":/icons/bookcase.svg" ), tr( "&Groups" ) );
    connect( groups, &Groups::showDictionaryInfo, this, &EditDictionaries::showDictionaryInfo );
    connect( orderAndProps, &OrderAndProps::showDictionaryHeadwords, this, &EditDictionaries::showDictionaryHeadwords );

    if ( noGroupEdits )
      origCfg.groups = groups->getGroups();

    if ( noOrderEdits )
      origCfg.dictionaryOrder = orderAndProps->getCurrentDictionaryOrder();

    if ( noInactiveEdits )
      origCfg.inactiveDictionaries = orderAndProps->getCurrentInactiveDictionaries();
  }
  ui.tabs->setUpdatesEnabled( true );
}
EditDictionaries::~EditDictionaries()
{
  disconnect( ui.tabs, &QTabWidget::currentChanged, this, &EditDictionaries::currentChanged );
}
