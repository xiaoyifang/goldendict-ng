/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "about.hh"
#include "version.hh"

#include <QClipboard>
#include <QPushButton>

About::About( QWidget * parent ):
  QDialog( parent )
{
  ui.setupUi( this );

  ui.versionInfo->setText( Version::everything() );

  connect( ui.copyInfoBtn, &QPushButton::clicked, [] {
    QGuiApplication::clipboard()->setText( Version::everything() );
  } );
}
