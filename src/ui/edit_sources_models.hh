/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once
#include "ui_edit_sources.h"
#include "config.hh"
#include "dict/hunspell.hh"
#include <QAbstractItemModel>
#include <QComboBox>
#include <QItemDelegate>
#include <QItemEditorFactory>

#include "texttospeechsource.hh"

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
// Forward declaration
class ChineseConversion;
#endif

/// A model to be projected into the mediawikis view, according to Qt's MVC model
class MediaWikisModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  MediaWikisModel( QWidget * parent, Config::MediaWikis const & );

  void removeWiki( int index );
  void addNewWiki();

  void remove(const QModelIndexList &);

  /// Returns the wikis the model currently has listed
  Config::MediaWikis const & getCurrentWikis() const
  {
    return mediawikis;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::MediaWikis mediawikis;
};

/// A model to be projected into the webSites view, according to Qt's MVC model
class WebSitesModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  WebSitesModel( QWidget * parent, Config::WebSites const & );

  void removeSite( int index );
  void addNewSite();

  void remove(const QModelIndexList &);

  /// Returns the sites the model currently has listed
  Config::WebSites const & getCurrentWebSites() const
  {
    return webSites;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::WebSites webSites;
};

/// A model to be projected into the dictServers view, according to Qt's MVC model
class DictServersModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  DictServersModel( QWidget * parent, Config::DictServers const & );

  void removeServer( int index );
  void addNewServer();

  void remove(const QModelIndexList &);

  /// Returns the sites the model currently has listed
  Config::DictServers const & getCurrentDictServers() const
  {
    return dictServers;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::DictServers dictServers;
};

/// A model to be projected into the programs view, according to Qt's MVC model
class ProgramsModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  ProgramsModel( QWidget * parent, Config::Programs const & );

  void removeProgram( int index );
  void addNewProgram();

  void remove(const QModelIndexList &);

  /// Returns the sites the model currently has listed
  Config::Programs const & getCurrentPrograms() const
  {
    return programs;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::Programs programs;
};

class ProgramTypeEditor: public QComboBox
{
  Q_OBJECT
  Q_PROPERTY( int type READ getType WRITE setType USER true )

public:
  explicit ProgramTypeEditor( QWidget * widget = nullptr );

  // Returns localized name for the given program type
  static QString getNameForType( int );

public:
  int getType() const;
  void setType( int );
};

/// A model to be projected into the paths view, according to Qt's MVC model
class PathsModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  PathsModel( QWidget * parent, Config::Paths const & );

  void removePath( int index );

  void addNewPath(QString const &);

  void remove(const QModelIndexList &);

  /// Returns the paths the model currently has listed
  Config::Paths const & getCurrentPaths() const
  {
    return paths;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::Paths paths;
};

/// A model to be projected into the soundDirs view, according to Qt's MVC model
class SoundDirsModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  SoundDirsModel( QWidget * parent, Config::SoundDirs const & );

  void removeSoundDir( int index );

  void addNewSoundDir(QString const &path, QString const &name);

  void removeSoundDirs(const QList<QModelIndex> &indexes);

  /// Returns the soundDirs the model currently has listed
  Config::SoundDirs const & getCurrentSoundDirs() const
  {
    return soundDirs;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::SoundDirs soundDirs;
};

/// A model to be projected into the hunspell dictionaries view, according to Qt's MVC model
class HunspellDictsModel: public QAbstractTableModel
{
  Q_OBJECT

public:

  HunspellDictsModel( QWidget * parent, Config::Hunspell const & );

  void changePath( QString const & newPath );

  /// Returns the dictionaries currently enabled
  Config::Hunspell::Dictionaries const & getEnabledDictionaries() const
  {
    return enabledDictionaries;
  }

  Qt::ItemFlags flags( QModelIndex const & index ) const override;
  int rowCount( QModelIndex const & parent ) const override;
  int columnCount( QModelIndex const & parent ) const override;
  QVariant headerData( int section, Qt::Orientation orientation, int role ) const override;
  QVariant data( QModelIndex const & index, int role ) const override;
  bool setData( QModelIndex const & index, const QVariant & value, int role ) override;

private:

  Config::Hunspell::Dictionaries enabledDictionaries;
  std::vector< HunspellMorpho::DataFiles > dataFiles;
};


class Sources: public QWidget
{
  Q_OBJECT

public:
  Sources( QWidget * parent, Config::Class const & );

  Config::Paths const & getPaths() const
  {
    return pathsModel.getCurrentPaths();
  }

  Config::SoundDirs const & getSoundDirs() const
  {
    return soundDirsModel.getCurrentSoundDirs();
  }

  Config::MediaWikis const & getMediaWikis() const
  {
    return mediawikisModel.getCurrentWikis();
  }

  Config::WebSites const & getWebSites() const
  {
    return webSitesModel.getCurrentWebSites();
  }

  Config::DictServers const & getDictServers() const
  {
    return dictServersModel.getCurrentDictServers();
  }

  Config::Programs const & getPrograms() const
  {
    return programsModel.getCurrentPrograms();
  }
#ifdef TTS_SUPPORT
  Config::VoiceEngines getVoiceEngines() const;
#endif
  Config::Hunspell getHunspell() const;

  Config::Transliteration getTransliteration() const;

  Config::Lingua getLingua() const;

  Config::Forvo getForvo() const;

signals:

  /// Emitted when a 'Rescan' button is clicked.
  void rescan();

private:
  Ui::Sources ui;

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  ChineseConversion * chineseConversion;
#endif
#ifdef TTS_SUPPORT
  TextToSpeechSource * textToSpeechSource;
#endif
  QItemDelegate * itemDelegate;
  QScopedPointer< QItemEditorFactory > itemEditorFactory;

  MediaWikisModel mediawikisModel;
  WebSitesModel webSitesModel;
  DictServersModel dictServersModel;
  ProgramsModel programsModel;
  PathsModel pathsModel;
  SoundDirsModel soundDirsModel;
  HunspellDictsModel hunspellDictsModel;

  void fitPathsColumns();
  void fitSoundDirsColumns();
  void fitHunspellDictsColumns();

private slots:

  void on_addPath_clicked();
  void on_removePath_clicked();

  void on_addSoundDir_clicked();
  void on_removeSoundDir_clicked();

  void on_changeHunspellPath_clicked();

  void on_addMediaWiki_clicked();
  void on_removeMediaWiki_clicked();

  void on_addWebSite_clicked();
  void on_removeWebSite_clicked();

  void on_removeDictServer_clicked();
  void on_addDictServer_clicked();

  void on_addProgram_clicked();
  void on_removeProgram_clicked();

  void on_rescan_clicked();
};
