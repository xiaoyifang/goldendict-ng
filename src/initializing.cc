/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QIcon>
#include "initializing.hh"
#include <QCloseEvent>

Initializing::Initializing( QWidget * parent, bool showSplashWindow ):
  QDialog( parent )
{
  ui.setupUi( this );

  setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
  setWindowIcon( QIcon( ":/icons/programicon.png" ) );

  if ( showSplashWindow ) {
    ui.operation->setText( tr( "Please wait..." ) );
    ui.dictionary->setText( "" );
    show();
  }
}

void Initializing::indexing( QString const & dictionaryName )
{
  ui.operation->setText( tr( "Indexing..." ) );
  ui.dictionary->setText( dictionaryName );
  show();
}

void Initializing::loading( QString const & dictionaryName )
{
  ui.operation->setText( tr( "Loading..." ) );
  ui.dictionary->setText( dictionaryName );
  show();
}

void Initializing::closeEvent( QCloseEvent * ev )
{
  ev->ignore();
}

void Initializing::reject() {}
