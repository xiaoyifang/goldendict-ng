#include "help.hh"
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace Help {
void openHelpWebpage( section sec )
{

  const auto main_url = QStringLiteral( "https://xiaoyifang.github.io/goldendict-ng/" );
  QUrl url;
  switch ( sec ) {
    case section::ui_fulltextserch:
      url = QUrl( main_url + QStringLiteral( "ui_fulltextsearch" ) );
      break;
    case section::ui_headwords:
      url = QUrl( main_url + QStringLiteral( "ui_headwords" ) );
      break;
    case section::ui_popup:
      url = QUrl( main_url + QStringLiteral( "ui_popup" ) );
      break;
    case section::manage_groups:
      url = QUrl( main_url + QStringLiteral( "manage_groups" ) );
      break;
    case section::manage_sources:
      url = QUrl( main_url + QStringLiteral( "manage_sources" ) );
      break;
    case section::main_website:
      url = QUrl( main_url );
      break;
    default:
      url = QUrl( main_url );
  }

  if ( !QDesktopServices::openUrl( url ) ) {
    QMessageBox msgBox;
    msgBox.setIcon( QMessageBox::Warning );
    msgBox.setText( QString( R"(Unable to open documentation.<br><a href="%1">%1</a>)" ).arg( main_url ) );
    msgBox.exec();
  }
}
} // namespace Help