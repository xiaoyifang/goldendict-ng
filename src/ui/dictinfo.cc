#include "dictinfo.hh"
#include "language.hh"
#include <QDesktopServices>
#include "config.hh"
#include "metadata.hh"
#include "common/utils.hh"
#include "common/globalbroadcaster.hh"
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
  currentDict = dict;
  setWindowTitle( QString::fromUtf8( dict->getName().data(), dict->getName().size() ) );

  ui.dictionaryId->setText( QString::fromStdString( dict->getId() ) );
  ui.enableFullindex->setText( dict->canFTS() ? tr( "Full-text search enabled" ) : tr( "Full-text search disabled" ) );
  ui.enableFullindex->setVisible( dict->isLocalDictionary() );
  ui.ftsToggleButton->setText( dict->canFTS() ? tr( "Disable" ) : tr( "Enable" ) );
  ui.ftsToggleButton->setVisible( dict->isLocalDictionary() );
  ui.dictionaryTotalArticles->setText( QString::number( dict->getArticleCount() ) );
  ui.dictionaryTotalWords->setText( QString::number( dict->getWordCount() ) );
  ui.dictionaryTranslatesFrom->setText( Language::localizedStringForId( dict->getLangFrom() ) );
  ui.dictionaryTranslatesTo->setText( Language::localizedStringForId( dict->getLangTo() ) );

  ui.openFolder->setVisible( dict->isLocalDictionary() );
  ui.openIndexFolder->setVisible( dict->isLocalDictionary() );

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

  ui.dictionaryFileLabel->setVisible( dict->isLocalDictionary() );
  ui.dictionaryFileList->setPlainText( filenamesText );
  ui.dictionaryFileList->setVisible( dict->isLocalDictionary() );

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

void DictInfo::on_ftsToggleButton_clicked()
{
  if ( !currentDict )
    return;

  bool currentState = currentDict->canFTS();
  bool newState     = !currentState;

  QString metadataPath = Utils::Path::combine( currentDict->getContainingFolder(), "metadata.toml" );
  Metadata::saveFullIndex( metadataPath.toStdString(), newState );

  currentDict->setFtsEnabled( newState );

  // Only stop indexing, MainWindow will handle delayed restart
  GlobalBroadcaster::instance()->stopFtsIndexing();

  ui.enableFullindex->setText( newState ? tr( "Full-text search enabled" ) : tr( "Full-text search disabled" ) );
  ui.ftsToggleButton->setText( newState ? tr( "Disable" ) : tr( "Enable" ) );
}
