#ifndef __PREFERENCES_HH_INCLUDED__
#define __PREFERENCES_HH_INCLUDED__

#include <QDialog>
#include <QAction>
#include "config.hh"
#include "ui_preferences.h"

/// Preferences dialog -- allows changing various program options.
class Preferences: public QDialog
{
  Q_OBJECT

  int prevInterfaceLanguage = 0;

  Config::CustomFonts prevWebFontFamily;

  Config::Class & cfg;
  QAction helpAction;

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

  void wholeAltClicked( bool );
  void wholeCtrlClicked( bool );
  void wholeShiftClicked( bool );

  void sideAltClicked( bool );
  void sideCtrlClicked( bool );
  void sideShiftClicked( bool );

  void on_enableMainWindowHotkey_toggled( bool checked );
  void on_enableClipboardHotkey_toggled( bool checked );

  void on_buttonBox_accepted();

  void on_useExternalPlayer_toggled( bool enabled );

  void customProxyToggled( bool );
  void on_maxNetworkCacheSize_valueChanged( int value );

  void on_collapseBigArticles_toggled( bool checked );
  void on_limitInputPhraseLength_toggled( bool checked );


  void on_font_standard_currentFontChanged(const QFont &f);
  void on_font_serif_currentFontChanged(const QFont &f);
  void on_font_sans_currentFontChanged(const QFont &f);
  void on_font_monospace_currentFontChanged(const QFont &f);
};

#endif

