#pragma once

#include <QToolBar>
#include <QSize>
#include <QList>
#include <QString>
#include <QTimer>
#include "dict/dictionary.hh"
#include "config.hh"

namespace Instances {
struct Group;
}

/// A bar containing dictionary icons of the currently chosen group.
/// Individual dictionaries can be toggled on and off.
class DictionaryBar: public QToolBar
{
  Q_OBJECT

public:

  /// Constructs an empty dictionary bar
  DictionaryBar( QWidget * parent, Config::Events &, const unsigned short & maxDictionaryRefsInContextMenu_ );

  /// Sets dictionaries to be displayed in the bar. Their statuses (enabled/
  /// disabled) are taken from the configuration data.
  void setDictionaries( const std::vector< sptr< Dictionary::Class > > & );
  void setMutedDictionaries( Config::MutedDictionaries * mutedDictionaries_ )
  {
    mutedDictionaries = mutedDictionaries_;
  }
  const Config::MutedDictionaries * getMutedDictionaries() const
  {
    return mutedDictionaries;
  }

  enum class IconSize {
    Small,
    Normal,
    Large,
  };

  void setDictionaryIconSize( IconSize size );
  void updateToGroup( const Instances::Group * grp,
                      Config::MutedDictionaries * allGroupMutedDictionaries,
                      Config::Class & cfg );

signals:

  /// Signalled when the user decided to edit group the bar currently
  /// shows.
  void editGroupRequested();

  /// Signal for show dictionary info command from context menu
  void showDictionaryInfo( const QString & id );

  /// Signal for show dictionary headwords command from context menu
  void showDictionaryHeadwords( Dictionary::Class * dict );

  /// Signal for open dictionary folder from context menu
  void openDictionaryFolder( const QString & id );

  /// Signal to close context menu
  void closePopupMenu();

private:

  Config::MutedDictionaries * mutedDictionaries;

  // In temporary selection, shift+click capture selections.
  std::optional< Config::MutedDictionaries > tempSelectionCapturedMuted;

  Config::Events & configEvents;

  void selectSingleDict( const QString & id );

  // how many dictionaries should be shown in the context menu:
  const unsigned short & maxDictionaryRefsInContextMenu;
  std::vector< sptr< Dictionary::Class > > allDictionaries;
  /// All the actions we have added to the toolbar
  QList< QAction * > dictActions;
  QAction * maxDictionaryRefsAction;

  QSize normalIconSize; // cache icon size set by stylesheet provided by user

protected:

  void contextMenuEvent( QContextMenuEvent * event );

private slots:

  void mutedDictionariesChanged();

  void actionWasTriggered( QAction * );

  void showContextMenu( QContextMenuEvent * event, bool extended = false );

public slots:

  void dictsPaneClicked( const QString & );
};
