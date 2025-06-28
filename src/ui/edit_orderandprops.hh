/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ui_edit_orderandprops.h"
#include "edit_groups_widgets.hh"
#include <QSortFilterProxyModel>

class OrderAndProps: public QWidget
{
  Q_OBJECT

public:

  OrderAndProps( QWidget * parent,
                 Config::Group const & dictionaryOrder,
                 Config::Group const & inactiveDictionaries,
                 std::vector< sptr< Dictionary::Class > > const & allDictionaries );
  void resetData( Config::Group const & dictionaryOrder,
                  Config::Group const & inactiveDictionaries,
                  std::vector< sptr< Dictionary::Class > > const & allDictionaries ) const;

  Config::Group getCurrentDictionaryOrder() const;
  Config::Group getCurrentInactiveDictionaries() const;

private slots:
  void dictionarySelectionChanged( const QItemSelection & current, const QItemSelection & deselected );
  void inactiveDictionarySelectionChanged( const QItemSelection & current );
  void contextMenuRequested( const QPoint & pos );
  void filterChanged( QString const & filterText );
  void dictListFocused();
  void inactiveDictListFocused();
  void showDictNumbers();

private:

  Ui::OrderAndProps ui;
  void disableDictionaryDescription();
  void describeDictionary( DictListWidget *, QModelIndex const & );

signals:
  void showDictionaryHeadwords( Dictionary::Class * dict );
};
