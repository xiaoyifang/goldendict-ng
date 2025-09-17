#include "dictinfo.hh"
#include "language.hh"
#include <QDesktopServices>
#include "config.hh"

#include <QString>

DictInfo::DictInfo( Config::Class & cfg_, QWidget * parent ):
  QDialog( parent ),
  cfg( cfg_ )
{
  ui.setupUi( this );
  if ( cfg.dictInfoGeometry.size() > 0 ) {
    restoreGeometry( cfg.dictInfoGeometry );
  }
  connect( this, &QDialog::finished, this, &DictInfo::savePos );
}

void DictInfo::showInfo( sptr< Dictionary::Class > dict )
{
  setWindowTitle( QString::fromUtf8( dict->getName().data(), dict->getName().size() ) );

  ui.dictionaryId->setText( QString::fromStdString( dict->getId() ) );
  ui.enableFullindex->setText( dict->canFTS() ? tr( "Full-text search enabled" ) : tr( "Full-text search disabled" ) );
  ui.dictionaryTotalArticles->setText( QString::number( dict->getArticleCount() ) );
  ui.dictionaryTotalWords->setText( QString::number( dict->getWordCount() ) );
  ui.dictionaryTranslatesFrom->setText( Language::localizedStringForId( dict->getLangFrom() ) );
  ui.dictionaryTranslatesTo->setText( Language::localizedStringForId( dict->getLangTo() ) );

  ui.openFolder->setVisible( dict->isLocalDictionary() );

  if ( dict->getWordCount() == 0 ) {
    ui.headwordsButton->setVisible( false );
  }
  else {
    ui.buttonsLayout->insertSpacerItem( 0, new QSpacerItem( 40, 20, QSizePolicy::Expanding ) );
  }

  const std::vector< std::string > & filenames = dict->getDictionaryFilenames();

  QString filenamesText;

  for ( const auto & filename : filenames ) {
    filenamesText += QString::fromStdString( filename );
    filenamesText += '\n';
  }

  ui.dictionaryFileList->setPlainText( filenamesText );

  if ( QString info = dict->getDescription(); !info.isEmpty() && info.compare( "NONE" ) != 0 ) {
    //qtbug QTBUG-112020
    info.remove( QRegularExpression( R"(<link[^>]*>)", QRegularExpression::CaseInsensitiveOption ) );
    ui.infoLabel->setHtml( info );
  }
  else {
    ui.infoLabel->clear();
  }

  setWindowIcon( dict->getIcon() );
}

void DictInfo::savePos( int )
{
  cfg.dictInfoGeometry = saveGeometry();
}

void DictInfo::on_openFolder_clicked()
{
  done( OPEN_FOLDER );
}

void DictInfo::on_OKButton_clicked()
{
  done( ACCEPTED );
}

void DictInfo::on_headwordsButton_clicked()
{
  done( SHOW_HEADWORDS );
}

void DictInfo::on_openIndexFolder_clicked()
{
  QDesktopServices::openUrl( QUrl::fromLocalFile( Config::getIndexDir() ) );
}
