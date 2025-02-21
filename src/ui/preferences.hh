#pragma once

#include <QDialog>
#include <QAction>
#include "config.hh"
#include "ui_preferences.h"

/// Preferences dialog -- allows changing various program options.
class Preferences: public QDialog
{
  Q_OBJECT

  int prevInterfaceLanguage = 0;

#if !defined( Q_OS_WIN )
  int prevInterfaceStyle = 0;
#endif


  Config::CustomFonts prevWebFontFamily;
  QString prevSysFont;
  int prevFontSize;

  Config::Class & cfg;
  QAction helpAction;
  void previewInterfaceFont( QString family, int size );

public:

  Preferences( QWidget * parent, Config::Class & cfg_ );
  void buildDisabledTypes( QString & disabledTypes, bool is_checked, QString name );
  ~Preferences() override = default;

  Config::Preferences getPreferences();

private:

  Ui::Preferences ui;

private slots:

  void enableScanPopupModifiersToggled( bool );
  void showScanFlagToggled( bool b );

  void on_enableMainWindowHotkey_toggled( bool checked );
  void on_enableClipboardHotkey_toggled( bool checked );

  void on_buttonBox_accepted();

  void on_useExternalPlayer_toggled( bool enabled );

  void customProxyToggled( bool );
  void on_maxNetworkCacheSize_valueChanged( int value );

  void on_collapseBigArticles_toggled( bool checked );
  void on_limitInputPhraseLength_toggled( bool checked );
};
