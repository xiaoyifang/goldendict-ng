/* This file is (c) 2013 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QWidget>
#include <QSize>
#include <QAbstractListModel>
#include <QListView>
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>

#include <config.hh>
#include "history.hh"
#include "delegate.hh"

/// A widget holding the contents of the 'History' docklet.
class HistoryPaneWidget: public QWidget
{
  Q_OBJECT

public:
  explicit HistoryPaneWidget( QWidget * parent = 0 ):
    QWidget( parent ),
    itemSelectionChanged( false ),
    listItemDelegate( 0 )
  {
  }
  virtual ~HistoryPaneWidget();

  virtual QSize sizeHint() const
  {
    return QSize( 204, 204 );
  }

  void setUp( Config::Class * cfg, History * history, QMenu * menu );

signals:
  void historyItemRequested( const QString & word );

public slots:
  void updateHistoryCounts();

private slots:
  void emitHistoryItemRequested( const QModelIndex & );
  void onSelectionChanged( const QItemSelection & selection, const QItemSelection & deselected );
  void onItemClicked( const QModelIndex & idx );
  void showCustomMenu( const QPoint & pos );
  void deleteSelectedItems();
  void copySelectedItems();

private:
  virtual bool eventFilter( QObject *, QEvent * );

  Config::Class * m_cfg               = nullptr;
  History * m_history                 = nullptr;
  QListView * m_historyList           = nullptr;
  QMenu * m_historyMenu               = nullptr;
  QAction * m_deleteSelectedAction    = nullptr;
  QAction * m_separator               = nullptr;
  QAction * m_copySelectedToClipboard = nullptr;

  QWidget historyPaneTitleBar;
  QHBoxLayout historyPaneTitleBarLayout;
  QLabel historyLabel;
  QLabel historyCountLabel;

  /// needed to avoid multiple notifications
  /// when selecting history items via mouse and keyboard
  bool itemSelectionChanged;

  WordListItemDelegate * listItemDelegate;
};

class HistoryModel: public QAbstractListModel
{
  Q_OBJECT

public:
  explicit HistoryModel( History * history, QObject * parent = 0 );

  int rowCount( const QModelIndex & parent = QModelIndex() ) const;

  QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const;

private slots:
  void historyChanged();

private:
  History * m_history;
};
