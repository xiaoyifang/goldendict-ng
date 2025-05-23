/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "edit_sources_models.hh"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  #include "chineseconversion.hh"
#endif


Sources::Sources( QWidget * parent, Config::Class const & cfg ):
  QWidget( parent ),
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  chineseConversion( new ChineseConversion( this, cfg.transliteration.chinese ) ),
#endif
#ifdef TTS_SUPPORT
  textToSpeechSource( nullptr ),
#endif
  itemDelegate( new QItemDelegate( this ) ),
  itemEditorFactory( new QItemEditorFactory() ),
  mediawikisModel( this, cfg.mediawikis ),
  webSitesModel( this, cfg.webSites ),
  dictServersModel( this, cfg.dictServers ),
  programsModel( this, cfg.programs ),
  pathsModel( this, cfg.paths ),
  soundDirsModel( this, cfg.soundDirs ),
  hunspellDictsModel( this, cfg.hunspell )
{
  ui.setupUi( this );

  Config::Hunspell const & hunspell   = cfg.hunspell;
  Config::Transliteration const & trs = cfg.transliteration;

  Config::Lingua const & lingua = cfg.lingua;
  Config::Forvo const & forvo   = cfg.forvo;

  // itemEditorFactory owns the ProgramTypeEditor
  itemEditorFactory->registerEditor( QMetaType::Int, new QStandardItemEditorCreator< ProgramTypeEditor >() );

  itemDelegate->setItemEditorFactory( itemEditorFactory.get() );

  ui.mediaWikis->setTabKeyNavigation( true );
  ui.mediaWikis->setModel( &mediawikisModel );
  ui.mediaWikis->resizeColumnToContents( 0 );
  ui.mediaWikis->resizeColumnToContents( 1 );
  ui.mediaWikis->resizeColumnToContents( 2 );
  ui.mediaWikis->resizeColumnToContents( 3 );
  ui.mediaWikis->resizeColumnToContents( 4 );

  ui.webSites->setTabKeyNavigation( true );
  ui.webSites->setModel( &webSitesModel );
  //[As link] column.
  ui.webSites->setColumnHidden( 1, true );
  ui.webSites->resizeColumnToContents( 0 );
  ui.webSites->resizeColumnToContents( 1 );
  ui.webSites->resizeColumnToContents( 2 );
  ui.webSites->resizeColumnToContents( 3 );
  ui.webSites->resizeColumnToContents( 4 );

  ui.dictServers->setTabKeyNavigation( true );
  ui.dictServers->setModel( &dictServersModel );
  ui.dictServers->resizeColumnToContents( 0 );
  ui.dictServers->resizeColumnToContents( 1 );
  ui.dictServers->resizeColumnToContents( 2 );
  ui.dictServers->resizeColumnToContents( 3 );
  ui.dictServers->resizeColumnToContents( 4 );
  ui.dictServers->resizeColumnToContents( 5 );

  ui.programs->setTabKeyNavigation( true );
  ui.programs->setModel( &programsModel );
  ui.programs->resizeColumnToContents( 0 );
  // Make sure this thing will be large enough
  ui.programs->setColumnWidth(
    1,
    QFontMetrics( QFont() ).horizontalAdvance( ProgramTypeEditor::getNameForType( Config::Program::PrefixMatch ) )
      + 16 );
  ui.programs->resizeColumnToContents( 2 );
  ui.programs->resizeColumnToContents( 3 );
  ui.programs->resizeColumnToContents( 4 );
  ui.programs->setItemDelegate( itemDelegate );

  ui.paths->setTabKeyNavigation( true );
  ui.paths->setModel( &pathsModel );

  fitPathsColumns();

  ui.soundDirs->setTabKeyNavigation( true );
  ui.soundDirs->setModel( &soundDirsModel );

  fitSoundDirsColumns();

  ui.hunspellPath->setText( hunspell.dictionariesPath );
  ui.hunspellDictionaries->setTabKeyNavigation( true );
  ui.hunspellDictionaries->setModel( &hunspellDictsModel );

  fitHunspellDictsColumns();

  ui.enableRussianTransliteration->setChecked( trs.enableRussianTransliteration );
  ui.enableGermanTransliteration->setChecked( trs.enableGermanTransliteration );
  ui.enableGreekTransliteration->setChecked( trs.enableGreekTransliteration );
  ui.enableBelarusianTransliteration->setChecked( trs.enableBelarusianTransliteration );

#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  ui.transliterationLayout->addWidget( chineseConversion );
  ui.transliterationLayout->addItem( new QSpacerItem( 20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding ) );
#endif

  ui.enableRomaji->setChecked( trs.romaji.enable );
  ui.enableHiragana->setChecked( trs.romaji.enableHiragana );
  ui.enableKatakana->setChecked( trs.romaji.enableKatakana );

  ui.enableCustomTransliteration->setChecked( trs.customTrans.enable );
  ui.customTransliteration->setPlainText( trs.customTrans.context );

  ui.linguaEnabled->setChecked( lingua.enable );
  ui.linguaLangCode->setText( lingua.languageCodes );

  ui.forvoEnabled->setChecked( forvo.enable );
  ui.forvoApiKey->setText( forvo.apiKey );
  ui.forvoLanguageCodes->setText( forvo.languageCodes );

  // Text to speech
#ifdef TTS_SUPPORT
  if ( !cfg.notts ) {
    textToSpeechSource = new TextToSpeechSource( this, cfg.voiceEngines );
    ui.tabWidget->addTab( textToSpeechSource, QIcon( ":/icons/text2speech.svg" ), tr( "Text to Speech" ) );
  }
#endif
}

void Sources::fitPathsColumns()
{
  ui.paths->resizeColumnToContents( 0 );
  ui.paths->resizeColumnToContents( 1 );
}

void Sources::fitSoundDirsColumns()
{
  ui.soundDirs->resizeColumnToContents( 0 );
  ui.soundDirs->resizeColumnToContents( 1 );
  ui.soundDirs->resizeColumnToContents( 2 );
}

void Sources::fitHunspellDictsColumns()
{
  ui.hunspellDictionaries->resizeColumnToContents( 0 );
  ui.hunspellDictionaries->resizeColumnToContents( 1 );
}

void Sources::on_addPath_clicked()
{
  QString dir = QFileDialog::getExistingDirectory( this, tr( "Choose a directory" ) );

  if ( !dir.isEmpty() ) {
    pathsModel.addNewPath( dir );
    fitPathsColumns();
  }
}

void Sources::on_removePath_clicked()
{
  QModelIndex current = ui.paths->currentIndex();

  if ( current.isValid()
       && QMessageBox::question(
            this,
            tr( "Confirm removal" ),
            tr( "Remove directory <b>%1</b> from the list?" ).arg( pathsModel.getCurrentPaths()[ current.row() ].path ),
            QMessageBox::Ok,
            QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    pathsModel.removePath( current.row() );
    fitPathsColumns();
  }
}

void Sources::on_addSoundDir_clicked()
{
  QString dir = QFileDialog::getExistingDirectory( this, tr( "Choose a directory" ) );

  if ( !dir.isEmpty() ) {
    soundDirsModel.addNewSoundDir( dir, QDir( dir ).dirName() );
    fitSoundDirsColumns();
  }
}

void Sources::on_removeSoundDir_clicked()
{
  QModelIndex current = ui.soundDirs->currentIndex();

  if ( current.isValid()
       && QMessageBox::question( this,
                                 tr( "Confirm removal" ),
                                 tr( "Remove directory <b>%1</b> from the list?" )
                                   .arg( soundDirsModel.getCurrentSoundDirs()[ current.row() ].path ),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    soundDirsModel.removeSoundDir( current.row() );
    fitSoundDirsColumns();
  }
}

void Sources::on_changeHunspellPath_clicked()
{
  QString dir = QFileDialog::getExistingDirectory( this, tr( "Choose a directory" ) );

  if ( !dir.isEmpty() ) {
    ui.hunspellPath->setText( dir );
    hunspellDictsModel.changePath( dir );
    fitHunspellDictsColumns();
  }
}

void Sources::on_addMediaWiki_clicked()
{
  mediawikisModel.addNewWiki();
  QModelIndex result = mediawikisModel.index( mediawikisModel.rowCount( QModelIndex() ) - 1, 1, QModelIndex() );

  ui.mediaWikis->scrollTo( result );
  //ui.mediaWikis->setCurrentIndex( result );
  ui.mediaWikis->edit( result );
}

void Sources::on_removeMediaWiki_clicked()
{
  QModelIndex current = ui.mediaWikis->currentIndex();

  if ( current.isValid()
       && QMessageBox::question(
            this,
            tr( "Confirm removal" ),
            tr( "Remove site <b>%1</b> from the list?" ).arg( mediawikisModel.getCurrentWikis()[ current.row() ].name ),
            QMessageBox::Ok,
            QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    mediawikisModel.removeWiki( current.row() );
  }
}

void Sources::on_addWebSite_clicked()
{
  webSitesModel.addNewSite();

  QModelIndex result = webSitesModel.index( webSitesModel.rowCount( QModelIndex() ) - 1, 1, QModelIndex() );

  ui.webSites->scrollTo( result );
  ui.webSites->edit( result );
}

void Sources::on_removeWebSite_clicked()
{
  QModelIndex current = ui.webSites->currentIndex();

  if ( current.isValid()
       && QMessageBox::question( this,
                                 tr( "Confirm removal" ),
                                 tr( "Remove site <b>%1</b> from the list?" )
                                   .arg( webSitesModel.getCurrentWebSites()[ current.row() ].name ),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    webSitesModel.removeSite( current.row() );
  }
}

void Sources::on_addDictServer_clicked()
{
  dictServersModel.addNewServer();

  QModelIndex result = dictServersModel.index( dictServersModel.rowCount( QModelIndex() ) - 1, 1, QModelIndex() );

  ui.dictServers->scrollTo( result );
  ui.dictServers->edit( result );
}

void Sources::on_removeDictServer_clicked()
{
  QModelIndex current = ui.dictServers->currentIndex();

  if ( current.isValid()
       && QMessageBox::question( this,
                                 tr( "Confirm removal" ),
                                 tr( "Remove site <b>%1</b> from the list?" )
                                   .arg( dictServersModel.getCurrentDictServers()[ current.row() ].name ),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    dictServersModel.removeServer( current.row() );
  }
}

void Sources::on_addProgram_clicked()
{
  programsModel.addNewProgram();

  QModelIndex result = programsModel.index( programsModel.rowCount( QModelIndex() ) - 1, 1, QModelIndex() );

  ui.programs->scrollTo( result );
  ui.programs->edit( result );
}

void Sources::on_removeProgram_clicked()
{
  QModelIndex current = ui.programs->currentIndex();

  if ( current.isValid()
       && QMessageBox::question( this,
                                 tr( "Confirm removal" ),
                                 tr( "Remove program <b>%1</b> from the list?" )
                                   .arg( programsModel.getCurrentPrograms()[ current.row() ].name ),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    programsModel.removeProgram( current.row() );
  }
}

#ifdef TTS_SUPPORT
Config::VoiceEngines Sources::getVoiceEngines() const
{
  if ( !textToSpeechSource )
    return Config::VoiceEngines();
  return textToSpeechSource->getVoiceEnginesModel().getCurrentVoiceEngines();
}
#endif

Config::Hunspell Sources::getHunspell() const
{
  Config::Hunspell h;

  h.dictionariesPath    = ui.hunspellPath->text();
  h.enabledDictionaries = hunspellDictsModel.getEnabledDictionaries();

  return h;
}

Config::Transliteration Sources::getTransliteration() const
{
  Config::Transliteration tr;

  tr.enableRussianTransliteration    = ui.enableRussianTransliteration->isChecked();
  tr.enableGermanTransliteration     = ui.enableGermanTransliteration->isChecked();
  tr.enableGreekTransliteration      = ui.enableGreekTransliteration->isChecked();
  tr.enableBelarusianTransliteration = ui.enableBelarusianTransliteration->isChecked();
#ifdef MAKE_CHINESE_CONVERSION_SUPPORT
  chineseConversion->getConfig( tr.chinese );
#endif
  tr.romaji.enable            = ui.enableRomaji->isChecked();
  tr.romaji.enableHiragana    = ui.enableHiragana->isChecked();
  tr.romaji.enableKatakana    = ui.enableKatakana->isChecked();

  tr.customTrans.enable  = ui.enableCustomTransliteration->isChecked();
  tr.customTrans.context = ui.customTransliteration->toPlainText();

  return tr;
}

Config::Lingua Sources::getLingua() const
{
  Config::Lingua lingua;

  lingua.enable        = ui.linguaEnabled->isChecked();
  lingua.languageCodes = ui.linguaLangCode->text();

  return lingua;
}


Config::Forvo Sources::getForvo() const
{
  Config::Forvo forvo;

  forvo.enable        = ui.forvoEnabled->isChecked();
  forvo.apiKey        = ui.forvoApiKey->text();
  forvo.languageCodes = ui.forvoLanguageCodes->text();

  return forvo;
}

////////// MediaWikisModel

MediaWikisModel::MediaWikisModel( QWidget * parent, Config::MediaWikis const & mediawikis_ ):
  QAbstractTableModel( parent ),
  mediawikis( mediawikis_ )
{
}
void MediaWikisModel::removeWiki( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  mediawikis.erase( mediawikis.begin() + index );
  endRemoveRows();
}

void MediaWikisModel::addNewWiki()
{
  Config::MediaWiki w;

  w.enabled = false;

  w.id = Dictionary::generateRandomDictionaryId();

  w.url = "http://";

  w.lang = "";

  beginInsertRows( QModelIndex(), mediawikis.size(), mediawikis.size() );
  mediawikis.push_back( w );
  endInsertRows();
}


Qt::ItemFlags MediaWikisModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() ) {
    if ( !index.column() ) {
      result |= Qt::ItemIsUserCheckable;
    }
    else {
      result |= Qt::ItemIsEditable;
    }
  }

  return result;
}

int MediaWikisModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return mediawikis.size();
  }
}

int MediaWikisModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 5;
  }
}

QVariant MediaWikisModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Enabled" );
      case 1:
        return tr( "Name" );
      case 2:
        return tr( "Address" );
      case 3:
        return tr( "Icon" );
      case 4:
        return tr( "Language Variant" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant MediaWikisModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= mediawikis.size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        return mediawikis[ index.row() ].name;
      case 2:
        return mediawikis[ index.row() ].url;
      case 3:
        return mediawikis[ index.row() ].icon;
      case 4:
        return mediawikis[ index.row() ].lang;
      default:
        return QVariant();
    }
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    return mediawikis[ index.row() ].enabled ? Qt::Checked : Qt::Unchecked;
  }

  return QVariant();
}

bool MediaWikisModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= mediawikis.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    //qDebug( "type = %d", (int)value.type() );
    //qDebug( "value = %d", (int)value.toInt() );

    // XXX it seems to be always passing Int( 2 ) as a value, so we just toggle
    mediawikis[ index.row() ].enabled = !mediawikis[ index.row() ].enabled;

    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        mediawikis[ index.row() ].name = value.toString();
        dataChanged( index, index );
        return true;
      case 2:
        mediawikis[ index.row() ].url = value.toString();
        dataChanged( index, index );
        return true;
      case 3:
        mediawikis[ index.row() ].icon = value.toString();
        dataChanged( index, index );
        return true;
      case 4:
        mediawikis[ index.row() ].lang = value.toString();
        dataChanged( index, index );
        return true;
      default:
        return false;
    }
  }

  return false;
}


////////// WebSitesModel

WebSitesModel::WebSitesModel( QWidget * parent, Config::WebSites const & webSites_ ):
  QAbstractTableModel( parent ),
  webSites( webSites_ )
{
}
void WebSitesModel::removeSite( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  webSites.erase( webSites.begin() + index );
  endRemoveRows();
}

void WebSitesModel::addNewSite()
{
  Config::WebSite w;

  w.enabled = false;

  w.id = Dictionary::generateRandomDictionaryId();

  w.url = "http://";

  w.inside_iframe = true;

  beginInsertRows( QModelIndex(), webSites.size(), webSites.size() );
  webSites.push_back( w );
  endInsertRows();
}


Qt::ItemFlags WebSitesModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() ) {
    if ( index.column() <= 1 ) {
      result |= Qt::ItemIsUserCheckable;
    }
    else {
      result |= Qt::ItemIsEditable;
    }
  }

  return result;
}

int WebSitesModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return webSites.size();
  }
}

int WebSitesModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 5;
  }
}

QVariant WebSitesModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::ToolTipRole ) {
    if ( section == 1 ) {
      return tr( "Insert article as link inside <iframe> tag" );
    }

    return QVariant();
  }

  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Enabled" );
      case 1:
        return tr( "As link" );
      case 2:
        return tr( "Name" );
      case 3:
        return tr( "Address" );
      case 4:
        return tr( "Icon" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant WebSitesModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= webSites.size() ) {
    return QVariant();
  }

  if ( role == Qt::ToolTipRole ) {
    if ( index.column() == 1 ) {
      return tr( "Insert article as link inside <iframe> tag" );
    }

    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 2:
        return webSites[ index.row() ].name;
      case 3:
        return webSites[ index.row() ].url;
      case 4:
        return webSites[ index.row() ].iconFilename;
      default:
        return QVariant();
    }
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    return webSites[ index.row() ].enabled ? Qt::Checked : Qt::Unchecked;
  }

  if ( role == Qt::CheckStateRole && index.column() == 1 ) {
    return webSites[ index.row() ].inside_iframe ? Qt::Checked : Qt::Unchecked;
  }

  return QVariant();
}

bool WebSitesModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= webSites.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    //qDebug( "type = %d", (int)value.type() );
    //qDebug( "value = %d", (int)value.toInt() );

    // XXX it seems to be always passing Int( 2 ) as a value, so we just toggle
    webSites[ index.row() ].enabled = !webSites[ index.row() ].enabled;

    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::CheckStateRole && index.column() == 1 ) {
    webSites[ index.row() ].inside_iframe = !webSites[ index.row() ].inside_iframe;

    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 2:
        webSites[ index.row() ].name = value.toString();
        dataChanged( index, index );
        return true;
      case 3:
        webSites[ index.row() ].url = value.toString();
        dataChanged( index, index );
        return true;
      case 4:
        webSites[ index.row() ].iconFilename = value.toString();
        dataChanged( index, index );
        return true;
      default:
        return false;
    }
  }

  return false;
}

////////// DictServersModel

DictServersModel::DictServersModel( QWidget * parent, Config::DictServers const & dictServers_ ):
  QAbstractTableModel( parent ),
  dictServers( dictServers_ )
{
}
void DictServersModel::removeServer( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  dictServers.erase( dictServers.begin() + index );
  endRemoveRows();
}

void DictServersModel::addNewServer()
{
  Config::DictServer d;

  d.enabled = false;

  d.id = Dictionary::generateRandomDictionaryId();

  d.url = "dict://";

  beginInsertRows( QModelIndex(), dictServers.size(), dictServers.size() );
  dictServers.push_back( d );
  endInsertRows();
}

Qt::ItemFlags DictServersModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() ) {
    if ( !index.column() ) {
      result |= Qt::ItemIsUserCheckable;
    }
    else {
      result |= Qt::ItemIsEditable;
    }
  }

  return result;
}

int DictServersModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return dictServers.size();
  }
}

int DictServersModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 6;
  }
}

QVariant DictServersModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Enabled" );
      case 1:
        return tr( "Name" );
      case 2:
        return tr( "Address" );
      case 3:
        return tr( "Databases" );
      case 4:
        return tr( "Strategies" );
      case 5:
        return tr( "Icon" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant DictServersModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= dictServers.size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        return dictServers[ index.row() ].name;
      case 2:
        return dictServers[ index.row() ].url;
      case 3:
        return dictServers[ index.row() ].databases;
      case 4:
        return dictServers[ index.row() ].strategies;
      case 5:
        return dictServers[ index.row() ].iconFilename;
      default:
        return QVariant();
    }
  }

  if ( role == Qt::ToolTipRole && index.column() == 3 ) {
    return tr( "Comma-delimited list of databases\n(empty string or \"*\" matches all databases)" );
  }

  if ( role == Qt::ToolTipRole && index.column() == 4 ) {
    return tr( "Comma-delimited list of search strategies\n(empty string mean \"prefix\" strategy)" );
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    return dictServers[ index.row() ].enabled ? Qt::Checked : Qt::Unchecked;
  }

  return QVariant();
}

bool DictServersModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= dictServers.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    // XXX it seems to be always passing Int( 2 ) as a value, so we just toggle
    dictServers[ index.row() ].enabled = !dictServers[ index.row() ].enabled;

    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        dictServers[ index.row() ].name = value.toString();
        dataChanged( index, index );
        return true;
      case 2:
        dictServers[ index.row() ].url = value.toString();
        dataChanged( index, index );
        return true;
      case 3:
        dictServers[ index.row() ].databases = value.toString();
        dataChanged( index, index );
        return true;
      case 4:
        dictServers[ index.row() ].strategies = value.toString();
        dataChanged( index, index );
        return true;
      case 5:
        dictServers[ index.row() ].iconFilename = value.toString();
        dataChanged( index, index );
        return true;
      default:
        return false;
    }
  }

  return false;
}


////////// ProgramsModel

ProgramsModel::ProgramsModel( QWidget * parent, Config::Programs const & programs_ ):
  QAbstractTableModel( parent ),
  programs( programs_ )
{
}

void ProgramsModel::removeProgram( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  programs.erase( programs.begin() + index );
  endRemoveRows();
}

void ProgramsModel::addNewProgram()
{
  Config::Program p;

  p.enabled = false;
  p.type    = Config::Program::Audio;

  p.id = Dictionary::generateRandomDictionaryId();

  beginInsertRows( QModelIndex(), programs.size(), programs.size() );
  programs.push_back( p );
  endInsertRows();
}

Qt::ItemFlags ProgramsModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() ) {
    if ( !index.column() ) {
      result |= Qt::ItemIsUserCheckable;
    }
    else {
      result |= Qt::ItemIsEditable;
    }
  }

  return result;
}

int ProgramsModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return programs.size();
  }
}

int ProgramsModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 5;
  }
}

QVariant ProgramsModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Enabled" );
      case 1:
        return tr( "Type" );
      case 2:
        return tr( "Name" );
      case 3:
        return tr( "Command Line" );
      case 4:
        return tr( "Icon" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant ProgramsModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= programs.size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        if ( role == Qt::DisplayRole ) {
          return ProgramTypeEditor::getNameForType( programs[ index.row() ].type );
        }
        else {
          return QVariant( (int)programs[ index.row() ].type );
        }
      case 2:
        return programs[ index.row() ].name;
      case 3:
        return programs[ index.row() ].commandLine;
      case 4:
        return programs[ index.row() ].iconFilename;
      default:
        return QVariant();
    }
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    return programs[ index.row() ].enabled ? Qt::Checked : Qt::Unchecked;
  }

  return QVariant();
}

bool ProgramsModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= programs.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    programs[ index.row() ].enabled = !programs[ index.row() ].enabled;

    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case 1:
        programs[ index.row() ].type = Config::Program::Type( value.toInt() );
        dataChanged( index, index );
        return true;
      case 2:
        programs[ index.row() ].name = value.toString();
        dataChanged( index, index );
        return true;
      case 3:
        programs[ index.row() ].commandLine = value.toString();
        dataChanged( index, index );
        return true;
      case 4:
        programs[ index.row() ].iconFilename = value.toString();
        dataChanged( index, index );
        return true;
      default:
        return false;
    }
  }

  return false;
}

QString ProgramTypeEditor::getNameForType( int v )
{
  switch ( v ) {
    case Config::Program::Audio:
      return tr( "Audio" );
    case Config::Program::PlainText:
      return tr( "Plain Text" );
    case Config::Program::Html:
      return tr( "Html" );
    case Config::Program::PrefixMatch:
      return tr( "Prefix Match" );
    default:
      return tr( "Unknown" );
  }
}

ProgramTypeEditor::ProgramTypeEditor( QWidget * widget ):
  QComboBox( widget )
{
  for ( int x = 0; x < Config::Program::MaxTypeValue; ++x ) {
    addItem( getNameForType( x ) );
  }
}

int ProgramTypeEditor::getType() const
{
  return currentIndex();
}

void ProgramTypeEditor::setType( int t )
{
  setCurrentIndex( t );
}

////////// PathsModel

PathsModel::PathsModel( QWidget * parent, Config::Paths const & paths_ ):
  QAbstractTableModel( parent ),
  paths( paths_ )
{
}

void PathsModel::removePath( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  paths.erase( paths.begin() + index );
  endRemoveRows();
}

void PathsModel::addNewPath( QString const & path )
{
  beginInsertRows( QModelIndex(), paths.size(), paths.size() );
  paths.push_back( Config::Path( path, false ) );
  endInsertRows();
}

Qt::ItemFlags PathsModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( Config::isPortableVersion() ) {
    if ( index.isValid() && index.row() == 0 ) {
      result &= ~Qt::ItemIsSelectable;
      result &= ~Qt::ItemIsEnabled;
    }
  }

  if ( index.isValid() && index.column() == 1 ) {
    result |= Qt::ItemIsUserCheckable;
  }

  return result;
}

int PathsModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return paths.size();
  }
}

int PathsModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 2;
  }
}

QVariant PathsModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Path" );
      case 1:
        return tr( "Recursive" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant PathsModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= paths.size() ) {
    return QVariant();
  }

  if ( ( role == Qt::DisplayRole || role == Qt::EditRole ) && !index.column() ) {
    return paths[ index.row() ].path;
  }

  if ( role == Qt::CheckStateRole && index.column() == 1 ) {
    return paths[ index.row() ].recursive ? Qt::Checked : Qt::Unchecked;
  }

  return QVariant();
}

bool PathsModel::setData( QModelIndex const & index, const QVariant & /*value*/, int role )
{
  if ( index.row() >= paths.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && index.column() == 1 ) {
    paths[ index.row() ].recursive = !paths[ index.row() ].recursive;

    dataChanged( index, index );
    return true;
  }

  return false;
}


////////// SoundDirsModel

SoundDirsModel::SoundDirsModel( QWidget * parent, Config::SoundDirs const & soundDirs_ ):
  QAbstractTableModel( parent ),
  soundDirs( soundDirs_ )
{
}

void SoundDirsModel::removeSoundDir( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  soundDirs.erase( soundDirs.begin() + index );
  endRemoveRows();
}

void SoundDirsModel::addNewSoundDir( QString const & path, QString const & name )
{
  beginInsertRows( QModelIndex(), soundDirs.size(), soundDirs.size() );
  soundDirs.push_back( Config::SoundDir( path, name ) );
  endInsertRows();
}

Qt::ItemFlags SoundDirsModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() && index.column() < 3 ) {
    result |= Qt::ItemIsEditable;
  }

  return result;
}

int SoundDirsModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return soundDirs.size();
  }
}

int SoundDirsModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 3;
  }
}

QVariant SoundDirsModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Path" );
      case 1:
        return tr( "Name" );
      case 2:
        return tr( "Icon" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant SoundDirsModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= soundDirs.size() ) {
    return QVariant();
  }

  if ( ( role == Qt::DisplayRole || role == Qt::EditRole ) && !index.column() ) {
    return soundDirs[ index.row() ].path;
  }

  if ( ( role == Qt::DisplayRole || role == Qt::EditRole ) && index.column() == 1 ) {
    return soundDirs[ index.row() ].name;
  }

  if ( ( role == Qt::DisplayRole || role == Qt::EditRole ) && index.column() == 2 ) {
    return soundDirs[ index.row() ].iconFilename;
  }

  return QVariant();
}

bool SoundDirsModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= soundDirs.size() ) {
    return false;
  }

  if ( ( role == Qt::DisplayRole || role == Qt::EditRole ) && index.column() < 3 ) {
    if ( !index.column() ) {
      soundDirs[ index.row() ].path = value.toString();
    }
    else if ( index.column() == 1 ) {
      soundDirs[ index.row() ].name = value.toString();
    }
    else {
      soundDirs[ index.row() ].iconFilename = value.toString();
    }

    dataChanged( index, index );
    return true;
  }

  return false;
}


////////// HunspellDictsModel

HunspellDictsModel::HunspellDictsModel( QWidget * parent, Config::Hunspell const & hunspell ):
  QAbstractTableModel( parent ),
  enabledDictionaries( hunspell.enabledDictionaries )
{
  changePath( hunspell.dictionariesPath );
}

void HunspellDictsModel::changePath( QString const & newPath )
{
  dataFiles = HunspellMorpho::findDataFiles( newPath );
  beginResetModel();
  endResetModel();
}

Qt::ItemFlags HunspellDictsModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractTableModel::flags( index );

  if ( index.isValid() ) {
    if ( !index.column() ) {
      result |= Qt::ItemIsUserCheckable;
    }
  }

  return result;
}

int HunspellDictsModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return dataFiles.size();
  }
}

int HunspellDictsModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() ) {
    return 0;
  }
  else {
    return 2;
  }
}

QVariant HunspellDictsModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case 0:
        return tr( "Enabled" );
      case 1:
        return tr( "Name" );
      default:
        return QVariant();
    }
  }

  return QVariant();
}

QVariant HunspellDictsModel::data( QModelIndex const & index, int role ) const
{
  if ( (unsigned)index.row() >= dataFiles.size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole && index.column() == 1 ) {
    return dataFiles[ index.row() ].dictName;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    for ( unsigned x = enabledDictionaries.size(); x--; ) {
      if ( enabledDictionaries[ x ] == dataFiles[ index.row() ].dictId ) {
        return Qt::Checked;
      }
    }

    return Qt::Unchecked;
  }

  return QVariant();
}

bool HunspellDictsModel::setData( QModelIndex const & index, const QVariant & /*value*/, int role )
{
  if ( (unsigned)index.row() >= dataFiles.size() ) {
    return false;
  }

  if ( role == Qt::CheckStateRole && !index.column() ) {
    for ( unsigned x = enabledDictionaries.size(); x--; ) {
      if ( enabledDictionaries[ x ] == dataFiles[ index.row() ].dictId ) {
        // Remove it now
        enabledDictionaries.erase( enabledDictionaries.begin() + x );
        dataChanged( index, index );
        return true;
      }
    }

    // Add it

    enabledDictionaries.push_back( dataFiles[ index.row() ].dictId );

    dataChanged( index, index );
    return true;
  }

  return false;
}

void Sources::on_rescan_clicked()
{
  emit rescan();
}
