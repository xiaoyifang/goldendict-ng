/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "config.hh"
#include "dict/dictionary.hh"
#include "edit_sources_models.hh"
#include "edit_groups.hh"
#include "instances.hh"
#include "edit_orderandprops.hh"
#include "ui_edit_dictionaries.h"
#include <QAction>
#include <QNetworkAccessManager>
#include <QPointer>

class EditDictionaries: public QDialog
{
  Q_OBJECT

public:

  EditDictionaries( QWidget * parent,
                    Config::Class & cfg,
                    std::vector< sptr< Dictionary::Class > > & dictionaries,
                    GroupInstances & groupInstances, // We only clear those on rescan
                    QNetworkAccessManager & dictNetMgr );

  ~EditDictionaries();

  /// Instructs the dialog to position itself on editing the given group.
  void editGroup( unsigned id );

  /// Returns true if any changes to the 'dictionaries' vector passed were done.
  bool areDictionariesChanged() const
  {
    return dictionariesChanged;
  }

  /// Returns true if groups were changed.
  bool areGroupsChanged() const
  {
    return groupsChanged;
  }

protected:

  virtual void accept();

private slots:

  void currentChanged( int index );

  void buttonBoxClicked( QAbstractButton * button );

  void rescanSources();

signals:

  void showDictionaryInfo( QString const & dictId );

  void showDictionaryHeadwords( Dictionary::Class * dict );

private:

  bool isSourcesChanged() const;

  void acceptChangedSources( bool rebuildGroups );

  //the rebuildGroups was an initative,means to build the group if possible.
  void save( bool rebuildGroups = false );

private:

  Config::Class & cfg;
  std::vector< sptr< Dictionary::Class > > & dictionaries;
  GroupInstances & groupInstances;
  QNetworkAccessManager & dictNetMgr;

  // Backed up to decide later if something was changed or not
  Config::GroupBackup origGroups;

  Ui::EditDictionaries ui;
  Sources sources;
  QPointer< OrderAndProps > orderAndProps;
  QPointer< Groups > groups;

  bool dictionariesChanged;
  bool groupsChanged;

  QString lastTabName;

  QAction helpAction;
};
