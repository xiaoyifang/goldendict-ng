/* This file is (c) 2013 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QApplication>
#include <QDockWidget>
#include <QKeyEvent>
#include <QClipboard>
#include <algorithm>
#include <functional>

#include "historypanewidget.hh"

void HistoryPaneWidget::setUp( Config::Class * cfg, History * history, QMenu * menu )
{
  m_cfg                     = cfg;
  m_history                 = history;
  m_historyList             = findChild< QListView * >( "historyList" );
  QDockWidget * historyPane = qobject_cast< QDockWidget * >( parentWidget() );

  // Delete selected items action
  m_deleteSelectedAction = new QAction( this );
  m_deleteSelectedAction->setText( tr( "&Delete Selected" ) );
  m_deleteSelectedAction->setShortcut( QKeySequence( QKeySequence::Delete ) );
  m_deleteSelectedAction->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( m_deleteSelectedAction );
  connect( m_deleteSelectedAction, &QAction::triggered, this, &HistoryPaneWidget::deleteSelectedItems );

  // Copy selected items to clipboard
  m_copySelectedToClipboard = new QAction( this );
  m_copySelectedToClipboard->setText( tr( "Copy Selected" ) );
  m_copySelectedToClipboard->setShortcut( QKeySequence( QKeySequence::Copy ) );
  m_copySelectedToClipboard->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  addAction( m_copySelectedToClipboard );
  connect( m_copySelectedToClipboard, &QAction::triggered, this, &HistoryPaneWidget::copySelectedItems );


  // Handle context menu, reusing some of the top-level window's History menu
  m_historyMenu = new QMenu( this );
  m_separator   = m_historyMenu->addSeparator();
  for ( const auto & a : menu->actions() ) {
    m_historyMenu->addAction( a );
  }

  // Make the history pane's titlebar

  historyLabel.setText( tr( "History:" ) );
  historyLabel.setObjectName( "historyLabel" );
  historyCountLabel.setObjectName( "historyCountLabel" );
  if ( layoutDirection() == Qt::LeftToRight ) {
    historyLabel.setAlignment( Qt::AlignLeft );
    historyCountLabel.setAlignment( Qt::AlignRight );
  }
  else {
    historyLabel.setAlignment( Qt::AlignRight );
    historyCountLabel.setAlignment( Qt::AlignLeft );
  }
  updateHistoryCounts();

  historyPaneTitleBarLayout.addWidget( &historyLabel );
  historyPaneTitleBarLayout.addWidget( &historyCountLabel );
  historyPaneTitleBarLayout.setContentsMargins( 5, 5, 5, 5 );
  historyPaneTitleBar.setLayout( &historyPaneTitleBarLayout );
  historyPaneTitleBar.setObjectName( "historyPaneTitleBar" );
  historyPane->setTitleBarWidget( &historyPaneTitleBar );

  // History list
  HistoryModel * historyModel = new HistoryModel( m_history, this );
  m_historyList->setModel( historyModel );
  m_historyList->setContextMenuPolicy( Qt::CustomContextMenu );
  // very important call, for performance reasons:
  m_historyList->setUniformItemSizes( true );
  m_historyList->setSelectionMode( QAbstractItemView::ExtendedSelection );
  m_historyList->installEventFilter( this );
  m_historyList->viewport()->installEventFilter( this );

  // list selection and keyboard navigation
  connect( m_historyList, &QAbstractItemView::clicked, this, &HistoryPaneWidget::onItemClicked );
  connect( m_history, &History::itemsChanged, this, &HistoryPaneWidget::updateHistoryCounts );
  connect( m_historyList->selectionModel(),
           &QItemSelectionModel::selectionChanged,
           this,
           &HistoryPaneWidget::onSelectionChanged );

  connect( m_historyList, &QWidget::customContextMenuRequested, this, &HistoryPaneWidget::showCustomMenu );

  listItemDelegate = new WordListItemDelegate( m_historyList->itemDelegate() );
  m_historyList->setItemDelegate( listItemDelegate );
}

HistoryPaneWidget::~HistoryPaneWidget()
{
  if ( listItemDelegate ) {
    delete listItemDelegate;
  }
}

void HistoryPaneWidget::copySelectedItems()
{
  QModelIndexList selectedIdxs = m_historyList->selectionModel()->selectedIndexes();

  if ( selectedIdxs.isEmpty() ) {
    // nothing to do
    return;
  }

  QStringList selectedStrings;
  for ( const auto & id : selectedIdxs ) {
    selectedStrings << m_historyList->model()->data( id ).toString();
  }

  QApplication::clipboard()->setText( selectedStrings.join( QString::fromLatin1( "\n" ) ) );
}

void HistoryPaneWidget::deleteSelectedItems()
{
  QModelIndexList selectedIdxs = m_historyList->selectionModel()->selectedIndexes();

  if ( selectedIdxs.isEmpty() ) {
    // nothing to do
    return;
  }

  QList< int > idxsToDelete;

  for ( const auto & id : selectedIdxs ) {
    idxsToDelete << id.row();
  }

  // Need to sort indexes in the decreasing order so that
  // the first deletions won't affect the indexes for subsequent deletions.
  std::sort( idxsToDelete.begin(), idxsToDelete.end(), std::greater< int >() );

  for ( const auto & id : idxsToDelete ) {
    m_history->removeItem( id );
  }

  if ( idxsToDelete.size() == 1 ) {
    // We've just removed a single entry,
    // keep the selection at the same index.
    m_historyList->setCurrentIndex( selectedIdxs.front() );
    m_historyList->selectionModel()->select( selectedIdxs.front(), QItemSelectionModel::SelectCurrent );
  }
  else {
    // Too many deletions, better to reset the selection.
    m_historyList->selectionModel()->reset();
  }
}

bool HistoryPaneWidget::eventFilter( QObject * obj, QEvent * ev )
{
  // unused for now

  return QWidget::eventFilter( obj, ev );
}

void HistoryPaneWidget::showCustomMenu( QPoint const & pos )
{
  bool selectionEmpty = m_historyList->selectionModel()->selection().empty();

  m_historyMenu->removeAction( m_copySelectedToClipboard );
  m_historyMenu->removeAction( m_deleteSelectedAction );

  m_separator->setVisible( !selectionEmpty );

  if ( !selectionEmpty ) {
    m_historyMenu->insertAction( m_separator, m_copySelectedToClipboard );
    m_historyMenu->insertAction( m_separator, m_deleteSelectedAction );
  }

  m_historyMenu->exec( m_historyList->mapToGlobal( pos ) );
}

void HistoryPaneWidget::emitHistoryItemRequested( QModelIndex const & idx )
{
  QVariant value = m_historyList->model()->data( idx );
  if ( !value.isNull() ) {
    emit historyItemRequested( value.toString() );
  }
}

void HistoryPaneWidget::onSelectionChanged( const QItemSelection & selection, const QItemSelection & deselected )
{
  Q_UNUSED( deselected );

  if ( selection.empty() ) {
    return;
  }

  itemSelectionChanged = true;
  emitHistoryItemRequested( selection.front().topLeft() );
}

void HistoryPaneWidget::onItemClicked( QModelIndex const & idx )
{
  // qDebug() << "clicked";

  if ( !itemSelectionChanged ) {
    emitHistoryItemRequested( idx );
  }
  itemSelectionChanged = false;
}

void HistoryPaneWidget::updateHistoryCounts()
{
  historyCountLabel.setText( tr( "%1/%2" ).arg( m_history->size() ).arg( m_cfg->preferences.maxStringsInHistory ) );
  historyCountLabel.setToolTip( tr( "History size: %1 entries out of maximum %2" )
                                  .arg( m_history->size() )
                                  .arg( m_cfg->preferences.maxStringsInHistory ) );
}

HistoryModel::HistoryModel( History * history, QObject * parent ):
  QAbstractListModel( parent ),
  m_history( history )
{

  connect( m_history, &History::itemsChanged, this, &HistoryModel::historyChanged );
}

int HistoryModel::rowCount( QModelIndex const & /*parent*/ ) const
{
  return m_history->size();
}

QVariant HistoryModel::data( QModelIndex const & index, int role ) const
{
  // qDebug() << "data: " << index;

  if ( !index.isValid() || index.row() >= m_history->size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::ToolTipRole ) {
    return m_history->getItem( index.row() ).word;
  }
  else {
    return QVariant();
  }
}

void HistoryModel::historyChanged()
{
  //  qDebug() << "History Changed!!";

  beginResetModel();
  endResetModel();
}
