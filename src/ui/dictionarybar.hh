#pragma once

#include <QToolBar>
#include <QSize>
#include <QList>
#include <QString>
#include <QTimer>
#include "dict/dictionary.hh"
#include "config.hh"

/// A bar containing dictionary icons of the currently chosen group.
/// Individual dictionaries can be toggled on and off.
class DictionaryBar: public QToolBar
{
  Q_OBJECT

public:

  /// Constructs an empty dictionary bar
  DictionaryBar( QWidget * parent, Config::Events &, unsigned short const & maxDictionaryRefsInContextMenu_ );

  /// Sets dictionaries to be displayed in the bar. Their statuses (enabled/
  /// disabled) are taken from the configuration data.
  void setDictionaries( std::vector< sptr< Dictionary::Class > > const & );
  void setMutedDictionaries( Config::MutedDictionaries * mutedDictionaries_ )
  {
    mutedDictionaries = mutedDictionaries_;
  }
  Config::MutedDictionaries const * getMutedDictionaries() const
  {
    return mutedDictionaries;
  }

  enum class IconSize {
    Small,
    Normal,
    Large,
  };

  void setDictionaryIconSize( IconSize size );

signals:

  /// Signalled when the user decided to edit group the bar currently
  /// shows.
  void editGroupRequested();

  /// Signal for show dictionary info command from context menu
  void showDictionaryInfo( QString const & id );

  /// Signal for show dictionary headwords command from context menu
  void showDictionaryHeadwords( Dictionary::Class * dict );

  /// Signal for open dictionary folder from context menu
  void openDictionaryFolder( QString const & id );

  /// Signal to close context menu
  void closePopupMenu();

private:

  Config::MutedDictionaries * mutedDictionaries;
  Config::Events & configEvents;
  Config::MutedDictionaries storedMutedSet;

  bool enterSoloMode = false;

  // how many dictionaries should be shown in the context menu:
  unsigned short const & maxDictionaryRefsInContextMenu;
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

  void dictsPaneClicked( QString const & );
};
