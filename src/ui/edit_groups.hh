/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ui_edit_groups.h"
#include "config.hh"
#include "dict/dictionary.hh"
#include <QToolButton>
#include <QMenu>

class Groups: public QWidget
{
  Q_OBJECT

public:
  Groups( QWidget * parent,
          std::vector< sptr< Dictionary::Class > > const &,
          Config::Groups const &,
          Config::Group const & order );
  void resetData( std::vector< sptr< Dictionary::Class > > const & dicts_,
                  Config::Groups const & groups_,
                  Config::Group const & order );
  /// Instructs the dialog to position itself on editing the given group.
  void editGroup( unsigned id );

  /// Should be called when the dictionary order has changed to reflect on
  /// that changes. It would only do anything if the order has actually
  /// changed.
  void updateDictionaryOrder( Config::Group const & order );

  Config::Groups getGroups() const;

private:
  Ui::Groups ui;
  std::vector< sptr< Dictionary::Class > > const & dicts;
  Config::Groups groups;

  QToolButton * groupsListButton;
  QMenu * groupsListMenu;

  // Reacts to the event that the number of groups is possibly changed
  void countChanged();

private slots:
  void addNew();
  void renameCurrent();
  void removeCurrent();
  void removeAll();
  void addToGroup();
  void removeFromGroup();

  /// Traditional Add Group by Language
  void addAutoGroups();
  /// by Containing Folder
  void addAutoGroupsByFolders();
  void groupsByMetadata();

  void showDictInfo( const QPoint & pos );
  void fillGroupsMenu();
  void switchToGroup( QAction * act );

signals:
  void showDictionaryInfo( QString const & id );
};
