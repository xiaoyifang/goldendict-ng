/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictheadwords.hh"
#include "gddebug.hh"
#include "headwordsmodel.hh"
#include <QDir>
#include <QFileDialog>
#include <QTimer>
#include <QProgressDialog>

#include <QRegularExpression>
#include "wildcard.hh"
#include "help.hh"
#include <QMessageBox>
#include <QMutexLocker>
#include <memory>


enum SearchType {
  FixedString,
  Wildcard,
  Regex
};
DictHeadwords::DictHeadwords( QWidget * parent, Config::Class & cfg_, Dictionary::Class * dict_ ):
  QDialog( parent ),
  cfg( cfg_ ),
  dict( dict_ ),
  helpAction( this )
{
  ui.setupUi( this );

  const bool fromMainWindow = parent->objectName() == "MainWindow";

  if ( fromMainWindow )
    setAttribute( Qt::WA_DeleteOnClose, false );

  setWindowFlags( windowFlags() & ~Qt::WindowContextHelpButtonHint );

  if ( cfg.headwordsDialog.headwordsDialogGeometry.size() > 0 )
    restoreGeometry( cfg.headwordsDialog.headwordsDialogGeometry );

  ui.searchModeCombo->addItem( tr( "Text" ), SearchType::FixedString );
  ui.searchModeCombo->addItem( tr( "Wildcards" ), SearchType::Wildcard );
  ui.searchModeCombo->addItem( tr( "RegExp" ), SearchType::Regex );
  ui.searchModeCombo->setCurrentIndex( cfg.headwordsDialog.searchMode );

  ui.exportButton->setAutoDefault( false );
  ui.OKButton->setAutoDefault( false );
  ui.applyButton->setAutoDefault( true );
  ui.applyButton->setDefault( true );

  ui.matchCase->setChecked( cfg.headwordsDialog.matchCase );

  model = std::make_shared< HeadwordListModel >();
  model->setMaxFilterResults( ui.filterMaxResult->value() );
  proxy = new QSortFilterProxyModel( this );

  proxy->setSourceModel( model.get() );

  proxy->setSortCaseSensitivity( Qt::CaseInsensitive );
  proxy->setSortLocaleAware( true );
  proxy->setDynamicSortFilter( false );

  ui.headersListView->setModel( proxy );
  ui.headersListView->setEditTriggers( QAbstractItemView::NoEditTriggers );

  // very important call, for performance reasons:
  ui.headersListView->setUniformItemSizes( true );

  delegate = new WordListItemDelegate( ui.headersListView->itemDelegate() );
  if ( delegate )
    ui.headersListView->setItemDelegate( delegate );

  ui.autoApply->setChecked( cfg.headwordsDialog.autoApply );

  //arbitrary number.
  bool exceedLimit = model->totalCount() > HEADWORDS_MAX_LIMIT;
  ui.filterMaxResult->setEnabled( exceedLimit );

  connect( this, &QDialog::finished, this, &DictHeadwords::savePos );

  if ( !fromMainWindow ) {
    ui.helpButton->hide();
    connect( this, &DictHeadwords::closeDialog, this, &QDialog::accept );
  }
  else {

    helpAction.setShortcut( QKeySequence( "F1" ) );
    helpAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );

    connect( ui.helpButton, &QAbstractButton::clicked, &helpAction, &QAction::trigger );
    connect( &helpAction, &QAction::triggered, []() {
      Help::openHelpWebpage( Help::section::ui_headwords );
    } );

    addAction( &helpAction );
  }

  connect( ui.OKButton, &QAbstractButton::clicked, this, &DictHeadwords::okButtonClicked );
  connect( ui.exportButton, &QAbstractButton::clicked, this, &DictHeadwords::exportButtonClicked );
  connect( ui.applyButton, &QAbstractButton::clicked, this, &DictHeadwords::filterChanged );

  connect( ui.autoApply, &QCheckBox::stateChanged, this, &DictHeadwords::autoApplyStateChanged );

  connect( ui.filterLine, &QLineEdit::textChanged, this, &DictHeadwords::filterChangedInternal );
  connect( ui.searchModeCombo, &QComboBox::currentIndexChanged, this, &DictHeadwords::filterChangedInternal );
  connect( ui.matchCase, &QCheckBox::stateChanged, this, &DictHeadwords::filterChangedInternal );

  connect( ui.headersListView, &QAbstractItemView::clicked, this, &DictHeadwords::itemClicked );

  connect( proxy, &QAbstractItemModel::dataChanged, this, &DictHeadwords::showHeadwordsNumber );
  connect( ui.filterMaxResult, &QSpinBox::valueChanged, this, [ this ]( int _value ) {
    model->setMaxFilterResults( _value );
  } );

  ui.headersListView->installEventFilter( this );

  setup( dict_ );
}

DictHeadwords::~DictHeadwords()
{
  if ( delegate )
    delegate->deleteLater();
}

void DictHeadwords::setup( Dictionary::Class * dict_ )
{
  QApplication::setOverrideCursor( Qt::WaitCursor );

  dict = dict_;

  setWindowTitle( QString::fromUtf8( dict->getName().c_str() ) );

  const auto size                            = dict->getWordCount();
  std::shared_ptr< HeadwordListModel > other = std::make_shared< HeadwordListModel >();
  model.swap( other );
  model->setDict( dict );
  proxy->setSourceModel( model.get() );
  proxy->sort( 0 );
  filterChanged();

  ui.autoApply->setEnabled( true );
  ui.autoApply->setChecked( cfg.headwordsDialog.autoApply );

  ui.applyButton->setEnabled( !ui.autoApply->isChecked() );

  //arbitrary number.
  bool exceedLimit = model->totalCount() > HEADWORDS_MAX_LIMIT;
  ui.filterMaxResult->setEnabled( exceedLimit );

  setWindowIcon( dict->getIcon() );

  dictId = QString( dict->getId().c_str() );

  QApplication::restoreOverrideCursor();
}

void DictHeadwords::savePos()
{
  cfg.headwordsDialog.searchMode = ui.searchModeCombo->currentIndex();
  cfg.headwordsDialog.matchCase  = ui.matchCase->isChecked();
  cfg.headwordsDialog.autoApply = ui.autoApply->isChecked();
  cfg.headwordsDialog.headwordsDialogGeometry = saveGeometry();
}

bool DictHeadwords::eventFilter( QObject * obj, QEvent * ev )
{
  if ( obj == ui.headersListView && ev->type() == QEvent::KeyPress ) {
    auto * kev = dynamic_cast< QKeyEvent * >( ev );
    if ( kev->key() == Qt::Key_Return || kev->key() == Qt::Key_Enter ) {
      itemClicked( ui.headersListView->currentIndex() );
      return true;
    }
    else if ( kev->key() == Qt::Key_Up ) {
      auto index = ui.headersListView->currentIndex();
      if ( index.row() == 0 )
        return true;
      auto preIndex = ui.headersListView->model()->index( index.row() - 1, index.column() );
      ui.headersListView->setCurrentIndex( preIndex );
      return true;
    }
    else if ( kev->key() == Qt::Key_Down ) {
      auto index = ui.headersListView->currentIndex();
      //last row.
      if ( index.row() == ui.headersListView->model()->rowCount() - 1 )
        return true;
      auto preIndex = ui.headersListView->model()->index( index.row() + 1, index.column() );
      ui.headersListView->setCurrentIndex( preIndex );
      return true;
    }
  }
  return QDialog::eventFilter( obj, ev );
}

void DictHeadwords::okButtonClicked()
{
  savePos();
  emit closeDialog();
}

void DictHeadwords::reject()
{
  savePos();
  emit closeDialog();
}

void DictHeadwords::exportButtonClicked()
{
  saveHeadersToFile();
}

void DictHeadwords::filterChangedInternal()
{
  // emit signal in async manner, to avoid UI slowdown
  if ( ui.autoApply->isChecked() )
    QTimer::singleShot( 100, this, &DictHeadwords::filterChanged );
}

QRegularExpression DictHeadwords::getFilterRegex() const
{
  const SearchType syntax =
    static_cast< SearchType >( ui.searchModeCombo->itemData( ui.searchModeCombo->currentIndex() ).toInt() );

  QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
  if ( !ui.matchCase->isChecked() )
    options |= QRegularExpression::CaseInsensitiveOption;

  QString pattern;
  switch ( syntax ) {
    case SearchType::FixedString:
      pattern = QRegularExpression::escape( ui.filterLine->text() );
      break;
    case SearchType::Wildcard:
      pattern = wildcardsToRegexp( ui.filterLine->text() );
      break;
    default:
      pattern = ui.filterLine->text();
      break;
  }

  QRegularExpression regExp = QRegularExpression( pattern, options );

  if ( !regExp.isValid() ) {
    gdWarning( "Invalid regexp pattern: %s\n", pattern.toUtf8().data() );
  }
  return regExp;
}

void DictHeadwords::filterChanged()
{
  const QRegularExpression regExp = getFilterRegex();

  QApplication::setOverrideCursor( Qt::WaitCursor );

  model->setFilter( regExp );

  proxy->setFilterRegularExpression( regExp );
  proxy->sort( 0 );

  QApplication::restoreOverrideCursor();

  showHeadwordsNumber();
}

void DictHeadwords::itemClicked( const QModelIndex & index )
{
  QVariant value = proxy->data( index, Qt::DisplayRole );
  if ( value.canConvert< QString >() ) {
    QString headword = value.toString();
    emit headwordSelected( headword, dictId );
  }
}

void DictHeadwords::autoApplyStateChanged( int state )
{
  ui.applyButton->setEnabled( state == Qt::Unchecked );
}

void DictHeadwords::showHeadwordsNumber()
{
  if ( ui.filterLine->text().isEmpty() ) {
    ui.headersNumber->setText( tr( "Unique headwords total: %1." ).arg( QString::number( model->totalCount() ) ) );
  }
  else {
    ui.headersNumber->setText( tr( "Unique headwords total: %1, filtered(limited): %2" )
                                 .arg( QString::number( model->totalCount() ), QString::number( proxy->rowCount() ) ) );
  }
}

void DictHeadwords::exportAllWords( QProgressDialog & progress, QTextStream & out )
{
  if ( const QRegularExpression regExp = getFilterRegex(); regExp.isValid() && !regExp.pattern().isEmpty() ) {
    loadRegex( progress, out );
    return;
  }

  const int headwordsNumber = model->totalCount();

  QMutexLocker const _( &mutex );
  QSet< QString > allHeadwords;

  int totalCount = 0;
  for ( int i = 0; i < headwordsNumber && i < model->wordCount(); ++i ) {
    if ( progress.wasCanceled() )
      break;

    QVariant value = model->getRow( i );
    if ( !value.canConvert< QString >() )
      continue;

    allHeadwords.insert( value.toString() );
  }

  for ( const auto & item : allHeadwords ) {
    progress.setValue( totalCount++ );

    writeWordToFile( out, item );
  }

    // continue to write the remaining headword
    int nodeIndex  = model->getCurrentIndex();
    auto headwords = model->getRemainRows( nodeIndex );
    while ( !headwords.isEmpty() ) {
      if ( progress.wasCanceled() )
        break;

      for ( const auto & item : headwords ) {
        progress.setValue( totalCount++ );

        writeWordToFile( out, item );
      }


      headwords = model->getRemainRows( nodeIndex );
    }
}

void DictHeadwords::loadRegex( QProgressDialog & progress, QTextStream & out )
{
  const int headwordsNumber = model->totalCount();

  QMutexLocker const _( &mutex );
  QSet< QString > allHeadwords;

  int totalCount = 0;
  for ( int i = 0; i < model->wordCount(); ++i ) {
    if ( progress.wasCanceled() )
      break;

    QVariant value = model->getRow( i );
    if ( !value.canConvert< QString >() )
      continue;

    allHeadwords.insert( value.toString() );
  }

  progress.setMaximum( allHeadwords.size() );
  for ( const auto & item : allHeadwords ) {
    progress.setValue( totalCount++ );

    writeWordToFile( out, item );
  }
}

void DictHeadwords::saveHeadersToFile()
{
  QString exportPath;
  if ( cfg.headwordsDialog.headwordsExportPath.isEmpty() )
    exportPath = QDir::homePath();
  else {
    exportPath = QDir::fromNativeSeparators( cfg.headwordsDialog.headwordsExportPath );
    if ( !QDir( exportPath ).exists() )
      exportPath = QDir::homePath();
  }

  QString const fileName = QFileDialog::getSaveFileName( this,
                                                         tr( "Save headwords to file" ),
                                                         exportPath,
                                                         tr( "Text files (*.txt);;All files (*.*)" ) );
  if ( fileName.size() == 0 )
    return;

  QFile file( fileName );

  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    QMessageBox::critical( this, "GoldenDict", tr( "Can not open exported file" ) );
    return;
  }

  cfg.headwordsDialog.headwordsExportPath =
    QDir::toNativeSeparators( QFileInfo( fileName ).absoluteDir().absolutePath() );

  const int headwordsNumber = model->totalCount();

  QProgressDialog progress( tr( "Export headwords..." ), tr( "Cancel" ), 0, headwordsNumber, this );
  progress.setWindowModality( Qt::WindowModal );

  // Write UTF-8 BOM
  QTextStream out( &file );
  out.setGenerateByteOrderMark( true );
//qt 6 will use utf-8 default.
#if QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 )
  out.setCodec( "UTF-8" );
#endif

  exportAllWords( progress, out );

  file.close();

  progress.setValue( progress.maximum() );
  if ( progress.wasCanceled() ) {
    QMessageBox::warning( this, "GoldenDict", tr( "Export process is interrupted" ) );
    gdWarning( "Headers export error: %s", file.errorString().toUtf8().data() );
  }
  else {
    //completed.
    progress.close();
    QMessageBox::information( this, "GoldenDict", tr( "Export finished" ) );
  }
}
void DictHeadwords::writeWordToFile( QTextStream & out, const QString & word )
{
  QByteArray line = word.toUtf8();

  //usually the headword should not contain \r \n;
  line.replace( '\n', ' ' );
  line.replace( '\r', ' ' );

  //write one line
  out << line << Qt::endl;
}
