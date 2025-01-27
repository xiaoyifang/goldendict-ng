/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "about.hh"
#include "utils.hh"
#include "version.hh"

#include <QClipboard>
#include <QFile>
#include <QPushButton>
#include <QSysInfo>

About::About( QWidget * parent, std::vector< sptr< Dictionary::Class > > * dictonaries ):
  QDialog( parent )
{
  ui.setupUi( this );

  ui.version->setText( Version::version() );

  ui.qtVersion->setText( tr( "Based on Qt %1 (%2, %3)" )
                           .arg( QLatin1String( qVersion() ), Version::compiler, QSysInfo::currentCpuArchitecture() )
                         + " (Xapian inside)" );

  connect( ui.copyInfoBtn, &QPushButton::clicked, [] {
    QGuiApplication::clipboard()->setText( Version::everything() );
  } );

  connect( ui.copyDictListBtn, &QPushButton::clicked, [ = ] {
    QString tempDictList{};
    for ( auto dict : *dictonaries ) {
      tempDictList.append( QString::fromStdString( dict->getName() + "\n" ) );
    }
    QGuiApplication::clipboard()->setText( tempDictList );
  } );
}
