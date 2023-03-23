#include "scanpopup_bar.h"
#include <QSizePolicy>
#include <QComboBox>

ScanPopupBar::ScanPopupBar( QWidget * parent ):
  QWidget( parent ),
  groupList( new GroupComboBox( this ) ),
  translateBox( new TranslateBox( this ) ),
  queryError( new QLabel( this ) ),
  goBackButton( new QToolButton( this ) ),
  goForwardButton( new QToolButton( this ) ),
  pronounceButton( new QToolButton( this ) ),
  sendWordButton( new QToolButton( this ) ),
  sendWordToFavoritesButton( new QToolButton( this ) ),
  showDictionaryBar( new QToolButton( this ) ),
  onTopButton( new QToolButton( this ) ),
  pinButton( new QToolButton( this ) )
{
  setupIcons();
  setupShortcuts();
  retranslateUi();

  // state related

  pronounceButton->setAutoRaise( false );
  onTopButton->setCheckable( true );
  pinButton->setCheckable( true );

  /*
Containers | c1                          | c2                                               |
Layout     | h1                          | h2                                               |
Widgets    | groupcombo + translate line | dict related buttons SPACE window control buttons|
Sizing     | Expanding                   | Fixed size       |
*/

  // TODO: the groupcombo + translateline is duplicated with mainwindow's groupcombo+transline

  showDictionaryBar->setCheckable( true );

  // Layout

  layout = new QHBoxLayout( this );

  h1 = new QHBoxLayout( nullptr ); // parent will be set in layout.addlayout();
  h2 = new QHBoxLayout( nullptr );
  h1->addWidget( groupList );
  groupList->setSizeAdjustPolicy( QComboBox::AdjustToContents );
  h1->addWidget( translateBox );

  h2->addWidget( queryError );
  h2->addWidget( goBackButton );
  h2->addWidget( goForwardButton );
  h2->addWidget( pronounceButton );
  h2->addWidget( sendWordButton );
  h2->addWidget( sendWordToFavoritesButton );

  groupList->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );
  translateBox->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Minimum );
  h2->addSpacerItem( new QSpacerItem( 20, 0 ) );

  h2->addWidget( showDictionaryBar );
  h2->addWidget( onTopButton );
  h2->addWidget( pinButton );

  h1->setSpacing( 1 );
  h2->setSpacing( 1 );

  h1->setContentsMargins( 0, 0, 0, 0 );
  h2->setContentsMargins( 0, 0, 0, 0 );
  layout->setContentsMargins( 0, 0, 0, 0 );

  layout->addLayout( h1 );
  layout->addLayout( h2 );

  setLayout( layout );
}

void ScanPopupBar::setupIcons() const
{
  goBackButton->setIcon( QIcon( ":/icons/previous.svg" ) );
  goForwardButton->setIcon( QIcon( ":/icons/next.svg" ) );
  pronounceButton->setIcon( QIcon( ":/icons/playsound_full.png" ) );
  sendWordButton->setIcon( QIcon( ":/icons/programicon.png" ) );
  sendWordToFavoritesButton->setIcon( QIcon( ":/icons/star.svg" ) );
  showDictionaryBar->setIcon( QIcon( ":/icons/bookcase.svg" ) );
  onTopButton->setIcon( QIcon( ":/icons/ontop.svg" ) );
  pinButton->setIcon( QIcon( ":/icons/pushpin.svg" ) );
}

void ScanPopupBar::retranslateUi() const
{
  goBackButton->setToolTip( tr( "Back" ) );
  goForwardButton->setToolTip( tr( "Forward" ) );
  pronounceButton->setToolTip( tr( "Pronounce Word (Alt+S)" ) );
  sendWordButton->setToolTip( tr( "Send word to main window (Alt+W)" ) );
  sendWordToFavoritesButton->setToolTip( tr( "Add word to Favorites (Ctrl+E)" ) );
  showDictionaryBar->setToolTip( tr( "Shows or hides the dictionary bar" ) );
  onTopButton->setToolTip( tr( "Always stay on top of all other windows" ) );
  pinButton->setToolTip( tr( "Pin down the window so it would stay on screen" ) );
}

void ScanPopupBar::setupShortcuts() const
{
  pronounceButton->setShortcut(QKeySequence(tr("Alt+S")));
  sendWordButton->setShortcut(QKeySequence(tr("Alt+W")));
  sendWordToFavoritesButton->setShortcut(QKeySequence(tr("Ctrl+E")));
}
