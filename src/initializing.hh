/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ui_initializing.h"

class Initializing: public QDialog
{
  Q_OBJECT

public:

  Initializing( QWidget * parent, bool showSplashWindow = true );

public slots:

  void indexing( QString const & dictionaryName );
  void loading( const QString & dictionaryName );

private:

  virtual void closeEvent( QCloseEvent * );
  virtual void reject();

  Ui::Initializing ui;
};
