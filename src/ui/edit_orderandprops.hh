/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ui_edit_orderandprops.h"
#include "edit_groups_widgets.hh"

class OrderAndProps: public QWidget
{
  Q_OBJECT

public:

  OrderAndProps( QWidget * parent,
                 const Config::Group & dictionaryOrder,
                 const Config::Group & inactiveDictionaries,
                 const std::vector< sptr< Dictionary::Class > > & allDictionaries );
  void resetData( const Config::Group & dictionaryOrder,
                  const Config::Group & inactiveDictionaries,
                  const std::vector< sptr< Dictionary::Class > > & allDictionaries ) const;

  Config::Group getCurrentDictionaryOrder() const;
  Config::Group getCurrentInactiveDictionaries() const;

private slots:
  void dictionarySelectionChanged( const QItemSelection & current, const QItemSelection & deselected );
  void inactiveDictionarySelectionChanged( const QItemSelection & current );
  void contextMenuRequested( const QPoint & pos );
  void filterChanged( const QString & filterText );
  void dictListFocused();
  void inactiveDictListFocused();
  void showDictNumbers();

private:

  Ui::OrderAndProps ui;
  void disableDictionaryDescription();
  void describeDictionary( DictListWidget *, const QModelIndex & );

signals:
  void showDictionaryHeadwords( Dictionary::Class * dict );
};
