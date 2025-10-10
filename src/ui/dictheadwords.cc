/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictheadwords.hh"
#include "headwordsmodel.hh"
#include <QFileDialog>
#include <QTimer>
#include <QProgressDialog>
#include "dict/xapianheadwordsmodel.hh"

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

  if ( fromMainWindow ) {
    setAttribute( Qt::WA_DeleteOnClose, false );
  }

  setWindowFlags( windowFlags() & ~Qt::WindowContextHelpButtonHint );

  if ( cfg.headwordsDialog.headwordsDialogGeometry.size() > 0 ) {
    restoreGeometry( cfg.headwordsDialog.headwordsDialogGeometry );
  }

  ui.searchModeCombo->addItem( tr( "Text" ), SearchType::FixedString );
  ui.searchModeCombo->addItem( tr( "Wildcards" ), SearchType::Wildcard );
  ui.searchModeCombo->addItem( tr( "Regular Expression" ), SearchType::Regex );
  ui.searchModeCombo->setCurrentIndex( cfg.headwordsDialog.searchMode );

  ui.exportButton->setAutoDefault( false );
  ui.OKButton->setAutoDefault( false );
  ui.applyButton->setAutoDefault( true );
  ui.applyButton->setDefault( true );

  ui.matchCase->setChecked( cfg.headwordsDialog.matchCase );

  proxy = new QSortFilterProxyModel( this );

  proxy->setSortCaseSensitivity( Qt::CaseInsensitive );
  proxy->setSortLocaleAware( true );
  proxy->setDynamicSortFilter( false );

  ui.headersListView->setModel( proxy );
  ui.headersListView->setEditTriggers( QAbstractItemView::NoEditTriggers );

  // very important call, for performance reasons:
  ui.headersListView->setUniformItemSizes( true );

  delegate = new WordListItemDelegate( ui.headersListView->itemDelegate() );
  if ( delegate ) {
    ui.headersListView->setItemDelegate( delegate );
  }

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
    connect( &helpAction, &QAction::triggered, this, []() {
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


  ui.headersListView->installEventFilter( this );

  setup( dict_ );
}

DictHeadwords::~DictHeadwords()
{
  if ( delegate ) {
    delegate->deleteLater();
  }
}

void DictHeadwords::setup( Dictionary::Class * dict_ )
{
  QApplication::setOverrideCursor( Qt::WaitCursor );

  dict = dict_;

  setWindowTitle( QString::fromUtf8( dict->getName().c_str() ) );

  if (dict->hasXapianHeadwordStorage())
    {
      // If the dictionary uses Xapian for headword storage, use the new model.
      qDebug() << "Using XapianHeadwordListModel for" << QString::fromStdString(dict->getName());
      auto xapianModel = std::make_shared<XapianIndexing::XapianHeadwordListModel>();
      xapianModel->setDict(dict);
      model = xapianModel;
      // Connect the signal from the search box to the model's setFilter slot
      // connect(mySearchLineEdit, &QLineEdit::textChanged, xapianModel.get(), &XapianIndexing::XapianHeadwordListModel::setFilter);
    }
  else
    {
      auto btreeModel = std::make_shared<HeadwordListModel>();
      btreeModel->setMaxFilterResults( ui.filterMaxResult->value() );
      btreeModel->setDict( dict );
      model = btreeModel;
    
    }
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
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      connect( btreeModel.get(), &HeadwordListModel::numberPopulated, this, [ this ]( int _ ) {
        showHeadwordsNumber();
      } );
      connect( ui.filterMaxResult, &QSpinBox::valueChanged, this, [ btreeModel ]( int _value ) {
        btreeModel->setMaxFilterResults( _value );
      } );
  }

  connect( proxy, &QAbstractItemModel::dataChanged, this, &DictHeadwords::showHeadwordsNumber );
}

void DictHeadwords::savePos()
{
  cfg.headwordsDialog.searchMode              = ui.searchModeCombo->currentIndex();
  cfg.headwordsDialog.matchCase               = ui.matchCase->isChecked();
  cfg.headwordsDialog.autoApply               = ui.autoApply->isChecked();
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
      if ( index.row() == 0 ) {
        return true;
      }
      auto preIndex = ui.headersListView->model()->index( index.row() - 1, index.column() );
      ui.headersListView->setCurrentIndex( preIndex );
      return true;
    }
    else if ( kev->key() == Qt::Key_Down ) {
      auto index = ui.headersListView->currentIndex();
      //last row.
      if ( index.row() == ui.headersListView->model()->rowCount() - 1 ) {
        return true;
      }
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
  if ( ui.autoApply->isChecked() ) {
    QTimer::singleShot( 100, this, &DictHeadwords::filterChanged );
  }
}

QRegularExpression DictHeadwords::getFilterRegex() const
{
  const SearchType syntax =
    static_cast< SearchType >( ui.searchModeCombo->itemData( ui.searchModeCombo->currentIndex() ).toInt() );

  QRegularExpression::PatternOptions options = QRegularExpression::UseUnicodePropertiesOption;
  if ( !ui.matchCase->isChecked() ) {
    options |= QRegularExpression::CaseInsensitiveOption;
  }

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
    qWarning( "Invalid regexp pattern: %s", pattern.toUtf8().data() );
  }
  return regExp;
}

void DictHeadwords::filterChanged()
{
  const QRegularExpression regExp = getFilterRegex();

  QApplication::setOverrideCursor( Qt::WaitCursor );

  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      btreeModel->setFilter( regExp );
  }
  else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
      xapianModel->setFilter( ui.filterLine->text() );
  }

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
  quint32 totalCount = 0;
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      totalCount = btreeModel->totalCount();
  } else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
      totalCount = xapianModel->totalCount();
  }
  if ( ui.filterLine->text().isEmpty() ) {
    ui.headersNumber->setText( tr( "Unique headwords total: %1." ).arg( QString::number( totalCount ) ) );
  }
  else {
    ui.headersNumber->setText( tr( "Unique headwords total: %1, filtered(limited): %2" )
                                 .arg( QString::number( totalCount ), QString::number( proxy->rowCount() ) ) );
  }
}

void DictHeadwords::exportAllWords( QProgressDialog & progress, QTextStream & out )
{
  if ( const QRegularExpression regExp = getFilterRegex(); regExp.isValid() && !regExp.pattern().isEmpty() ) {
    loadRegex( progress, out );
    return;
  }

  quint32 headwordsNumber = 0;
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      headwordsNumber = btreeModel->totalCount();
  } else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
      headwordsNumber = xapianModel->totalCount();
  }

  const QMutexLocker _( &mutex );
  QSet< QString > allHeadwords;

  int totalCount = 0;
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      for ( int i = 0; i < headwordsNumber && i < btreeModel->wordCount(); ++i ) {
        if ( progress.wasCanceled() ) {
          break;
        }

        QVariant value = btreeModel->getRow( i );
        if ( !value.canConvert< QString >() ) {
          continue;
        }

        allHeadwords.insert( value.toString() );
      }

      for ( const auto & item : allHeadwords ) {
        progress.setValue( totalCount++ );

        writeWordToFile( out, item );
      }

      // continue to write the remaining headword
      int nodeIndex  = btreeModel->getCurrentIndex();
      auto headwords = btreeModel->getRemainRows( nodeIndex );
      while ( !headwords.isEmpty() ) {
        if ( progress.wasCanceled() ) {
          break;
        }

        for ( const auto & item : std::as_const( headwords ) ) {
          progress.setValue( totalCount++ );

          writeWordToFile( out, item );
        }

        headwords = btreeModel->getRemainRows( nodeIndex );
      }
  } else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
    // For Xapian model, fetch in pages and write directly to file to avoid high memory usage.
    quint32 offset = 0;
    quint32 total = 0;
    constexpr quint32 PageSize = 1000; // Use a larger page size for export

    do {
      std::map<QString, uint32_t> words = XapianIndexing::getIndexedWordsByOffset(dict->getId(), offset, PageSize, total);
      if (words.empty()) {
        break;
      }
      for (const auto& [word, _] : words) {
        if (progress.wasCanceled()) break;
        writeWordToFile(out, word);
        progress.setValue(++totalCount);
      }
      offset += words.size();
    } while (offset < total && !progress.wasCanceled());
  }
}

void DictHeadwords::loadRegex( QProgressDialog & progress, QTextStream & out )
{

  const QMutexLocker _( &mutex );
  QSet< QString > allHeadwords;

  int totalCount = 0;
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      for ( int i = 0; i < btreeModel->wordCount(); ++i ) {
        if ( progress.wasCanceled() ) {
          break;
        }

        QVariant value = btreeModel->getRow( i );
        if ( !value.canConvert< QString >() ) {
          continue;
        }

        allHeadwords.insert( value.toString() );
      }
  } else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
      // For Xapian model with filter, fetch in pages and add to set.
      // This still uses memory but is necessary for regex filtering on the client side.
      // A more advanced solution would be to push regex to the search engine if possible.
      quint32 offset = 0;
      quint32 total = 0;
      constexpr quint32 PageSize = 1000;

      do {
          std::map<QString, uint32_t> words = XapianIndexing::searchIndexedWordsByOffset(dict->getId(), ui.filterLine->text().toStdString(), offset, PageSize, total);
          if (words.empty()) {
              break;
          }
          for (const auto& [word, _] : words) {
              if (progress.wasCanceled()) break;
              allHeadwords.insert(word);
          }
          offset += words.size();
      } while (offset < total && !progress.wasCanceled());
      }
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
  if ( cfg.headwordsDialog.headwordsExportPath.isEmpty() ) {
    exportPath = QDir::homePath();
  }
  else {
    exportPath = QDir::fromNativeSeparators( cfg.headwordsDialog.headwordsExportPath );
    if ( !QDir( exportPath ).exists() ) {
      exportPath = QDir::homePath();
    }
  }

  const QString fileName = QFileDialog::getSaveFileName( this,
                                                         tr( "Save headwords to file" ),
                                                         exportPath,
                                                         tr( "Text files (*.txt);;All files (*.*)" ) );
  if ( fileName.size() == 0 ) {
    return;
  }

  QFile file( fileName );

  if ( !file.open( QFile::WriteOnly | QIODevice::Text ) ) {
    QMessageBox::critical( this, "GoldenDict", tr( "Can not open exported file" ) );
    return;
  }

  cfg.headwordsDialog.headwordsExportPath =
    QDir::toNativeSeparators( QFileInfo( fileName ).absoluteDir().absolutePath() );

  quint32 headwordsNumber = 0;
  if (auto btreeModel = std::dynamic_pointer_cast<HeadwordListModel>(model)) {
      headwordsNumber = btreeModel->totalCount();
  } else if (auto xapianModel = std::dynamic_pointer_cast<XapianIndexing::XapianHeadwordListModel>(model)) {
      headwordsNumber = xapianModel->totalCount();
  }


  QProgressDialog progress( tr( "Export headwords..." ), tr( "Cancel" ), 0, headwordsNumber, this );
  progress.setWindowModality( Qt::WindowModal );

  // Write UTF-8 BOM
  QTextStream out( &file );
  out.setGenerateByteOrderMark( true );

  exportAllWords( progress, out );

  file.close();

  progress.setValue( progress.maximum() );
  if ( progress.wasCanceled() ) {
    QMessageBox::warning( this, "GoldenDict", tr( "Export process is interrupted" ) );
    qWarning( "Headers export error: %s", file.errorString().toUtf8().data() );
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
