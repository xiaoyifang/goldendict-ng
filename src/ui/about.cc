/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "about.hh"
#include "utils.hh"

#include <QClipboard>
#include <QFile>
#include <QPushButton>
#include <QSysInfo>

About::About( QWidget * parent, std::vector< sptr< Dictionary::Class > > * dictonaries ): QDialog( parent )
{
  ui.setupUi( this );

  QFile versionFile( ":/version.txt" );

  QString version;

  if ( !versionFile.open( QFile::ReadOnly ) )
    version = tr( "[Unknown]" );
  else
    version = QString::fromLatin1( versionFile.readAll() ).trimmed();

  ui.version->setText( version );

#if defined (_MSC_VER)
  QString compilerVersion = QString( "Visual C++ Compiler: %1" )
                               .arg( _MSC_FULL_VER );
#elif defined (__clang__) && defined (__clang_version__)
  QString compilerVersion = QLatin1String( "Clang " ) + QLatin1String( __clang_version__ );
#else
  QString compilerVersion = QLatin1String( "GCC " ) + QLatin1String( __VERSION__ );
#endif

  ui.qtVersion->setText( tr( "Based on Qt %1 (%2, %3 bit)" ).arg(
                           QLatin1String( qVersion() ),
                           compilerVersion,
                           QString::number( QSysInfo::WordSize ) )
  +" (Xapian inside)"
                         );

  // copy basic debug info to clipboard
  connect(ui.copyInfoBtn, &QPushButton::clicked, [=]{
      QGuiApplication::clipboard()->setText(
          "Goldendict " + version + "\n" +
          QSysInfo::productType() + " " + QSysInfo::kernelType() + " " + QSysInfo::kernelVersion() + " " +
          "Qt " + QLatin1String(qVersion()) + " " +
          QSysInfo::buildAbi() + "\n" +
          compilerVersion + "\n"
    + "Flags:"
         +" USE_XAPIAN "

#ifdef MAKE_ZIM_SUPPORT
         +" MAKE_ZIM_SUPPORT"
#endif

#ifdef NO_EPWING_SUPPORT
         +" NO_EPWING_SUPPORT"
#endif

#ifdef USE_ICONV
        +" USE_ICONV"
#endif

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
         +" MAKE_CHINESE_CONVERSION_SUPPORT"
#endif
      );
  });

  connect(ui.copyDictListBtn, &QPushButton::clicked, [=]{
      QString tempDictList{};
      for (auto dict : *dictonaries) {
        tempDictList.append(QString::fromStdString(dict->getName() + "\n"));
      }
      QGuiApplication::clipboard()->setText(tempDictList);
  });

  QFile creditsFile( ":/CREDITS.txt" );

  if ( creditsFile.open( QFile::ReadOnly ) )
  {
    QStringList creditsList =
      QString::fromUtf8(
        creditsFile.readAll() ).split( '\n', Qt::SkipEmptyParts );

    QString html = "<html><body>";

    for( int x = 0; x < creditsList.size(); ++x )
    {
      QString str = creditsList[ x ];

      str.replace( "\\", "@" );

      str = Utils::escape( str );

      int colon = str.indexOf( ":" );

      if ( colon != -1 )
      {
        QString name( str.left( colon ) );

        name.replace( ", ", "<br>" );

        str = "<font color='blue'>" + name + "</font><br>&nbsp;&nbsp;&nbsp;&nbsp;"
              + str.mid( colon + 1 );
      }

      html += str;
      html += "<br>";
    }

    html += "</body></html>";

    ui.credits->setHtml( html );
  }
}
