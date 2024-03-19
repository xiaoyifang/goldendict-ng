/* This file is (c) 2013 Timon Wong <timon86.wang@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */
#ifndef NO_TTS_SUPPORT

#include "texttospeechsource.hh"
#include <QVariant>
#include <QMessageBox>
#include <memory>

TextToSpeechSource::TextToSpeechSource( QWidget * parent, Config::VoiceEngines voiceEngines ):
  QWidget( parent ),
  voiceEnginesModel( this, voiceEngines )
{
  ui.setupUi( this );

  SpeechClient::Engines engines = SpeechClient::availableEngines();

  ui.selectedVoiceEngines->setTabKeyNavigation( true );
  ui.selectedVoiceEngines->setModel( &voiceEnginesModel );
  ui.selectedVoiceEngines->hideColumn( VoiceEnginesModel::kColumnEngineName );
  fitSelectedVoiceEnginesColumns();
  ui.selectedVoiceEngines->setItemDelegateForColumn( VoiceEnginesModel::kColumnEngineDName,
                                                     new VoiceEngineItemDelegate( engines, this ) );

  foreach( Config::VoiceEngine ve, voiceEngines )
  {
    occupiedEngines.insert( ve.name );
  }

  foreach( SpeechClient::Engine engine, engines )
  {
    QMap< QString, QVariant > map;
    map[ "engine_name" ] = engine.engine_name;
    map[ "locale" ]      = engine.locale;
    map[ "voice_name" ]  = engine.voice_name;
    ui.availableVoiceEngines->addItem( engine.name, QVariant( map ) );
  }

  //disable the already added engines.
  // This is the effective 'disable' flag
  QVariant v( 0 );
  for ( int index = 0; index < ui.availableVoiceEngines->count(); index++ ) {
    auto name = ui.availableVoiceEngines->itemText( index );
    if ( occupiedEngines.contains( name ) ) {
      auto modelIndex = ui.availableVoiceEngines->model()->index( index, 0 );

      ui.availableVoiceEngines->model()->setData( modelIndex, v, Qt::UserRole - 1 );
    }
  }

  if ( voiceEngines.count() > 0 ) {
    QModelIndex const & idx = ui.selectedVoiceEngines->model()->index( 0, 0 );
    if ( idx.isValid() )
      ui.selectedVoiceEngines->setCurrentIndex( idx );
  }

  adjustSliders();

  connect( ui.volumeSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );
  connect( ui.rateSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );
  connect( ui.selectedVoiceEngines->selectionModel(),
           SIGNAL( selectionChanged( QItemSelection, QItemSelection ) ),
           this,
           SLOT( selectionChanged() ) );

  ui.availableVoiceEngines->view()->setVerticalScrollBarPolicy( Qt::ScrollBarAsNeeded );
}

void TextToSpeechSource::slidersChanged()
{
  if ( ui.selectedVoiceEngines->selectionModel()->hasSelection() )
    voiceEnginesModel.setEngineParams( ui.selectedVoiceEngines->currentIndex(),
                                       ui.volumeSlider->value(),
                                       ui.rateSlider->value() );
}

void TextToSpeechSource::on_addVoiceEngine_clicked()
{
  if ( ui.availableVoiceEngines->count() == 0 ) {
    QMessageBox::information(
      this,
      tr( "No TTS voice available" ),
      tr( "Cannot find available TTS voice.<br>"
          "Please make sure that at least one TTS engine installed on your computer already." ) );
    return;
  }

  int idx = ui.availableVoiceEngines->currentIndex();
  if ( idx >= 0 ) {
    QString name        = ui.availableVoiceEngines->itemText( idx );
    auto map            = ui.availableVoiceEngines->itemData( idx ).toMap();
    QString engine_name = map[ "engine_name" ].toString();
    QString locale      = map[ "locale" ].toString();
    QString voice_name  = map[ "voice_name" ].toString();
    voiceEnginesModel.addNewVoiceEngine( engine_name,
                                         QLocale( locale ),
                                         name,
                                         voice_name,
                                         ui.volumeSlider->value(),
                                         ui.rateSlider->value() );
    fitSelectedVoiceEnginesColumns();

    occupiedEngines.insert( name );
    auto modelIndex = ui.availableVoiceEngines->model()->index( idx, 0 );
    // This is the effective 'disable' flag
    QVariant v( 0 );
    ui.availableVoiceEngines->model()->setData( modelIndex, v, Qt::UserRole - 1 );
  }
}

void TextToSpeechSource::on_removeVoiceEngine_clicked()
{
  QModelIndex current = ui.selectedVoiceEngines->currentIndex();
  auto selected_name  = voiceEnginesModel.getCurrentVoiceEngines()[ current.row() ].name;

  if ( current.isValid()
       && QMessageBox::question( this,
                                 tr( "Confirm removal" ),
                                 tr( "Remove voice engine <b>%1</b> from the list?" ).arg( selected_name ),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel )
         == QMessageBox::Ok ) {
    voiceEnginesModel.removeVoiceEngine( current.row() );

    occupiedEngines.remove( selected_name );
    // This is the effective 'enable' flag
    QVariant v( 1 | 32 );

    //enable this engine.
    for ( int index = 0; index < ui.availableVoiceEngines->count(); index++ ) {
      auto name = ui.availableVoiceEngines->itemText( index );
      if ( name == selected_name ) {
        auto modelIndex = ui.availableVoiceEngines->model()->index( index, 0 );
        ui.availableVoiceEngines->model()->setData( modelIndex, v, Qt::UserRole - 1 );
        break;
      }
    }
  }
}

void TextToSpeechSource::on_previewVoice_clicked()
{
  int idx = ui.availableVoiceEngines->currentIndex();
  if ( idx < 0 )
    return;

  auto map         = ui.availableVoiceEngines->itemData( idx ).toMap();
  QString engineId = map[ "engine_name" ].toString();
  QString locale   = map[ "locale" ].toString();
  QString name     = ui.availableVoiceEngines->itemText( idx );
  QString text     = ui.previewText->text();
  int volume       = ui.volumeSlider->value();
  int rate         = ui.rateSlider->value();

  speechClient = std::make_unique< SpeechClient >(
    Config::VoiceEngine( engineId, name, map[ "voice_name" ].toString(), QLocale( locale ), volume, rate ),
    this );
  speechClient->tell( text );
}

void TextToSpeechSource::previewVoiceFinished()
{
  ui.previewVoice->setDisabled( false );
}

void TextToSpeechSource::fitSelectedVoiceEnginesColumns()
{
  ui.selectedVoiceEngines->resizeColumnToContents( VoiceEnginesModel::kColumnEnabled );
  ui.selectedVoiceEngines->resizeColumnToContents( VoiceEnginesModel::kColumnEngineDName );
  ui.selectedVoiceEngines->resizeColumnToContents( VoiceEnginesModel::kColumnIcon );
}

void TextToSpeechSource::adjustSliders()
{
  QModelIndex const & index = ui.selectedVoiceEngines->currentIndex();
  if ( index.isValid() ) {
    Config::VoiceEngines const & engines = voiceEnginesModel.getCurrentVoiceEngines();
    ui.volumeSlider->setValue( engines[ index.row() ].volume );
    ui.rateSlider->setValue( engines[ index.row() ].rate );
    return;
  }
  ui.volumeSlider->setValue( 50 );
  ui.rateSlider->setValue( 0 );
}

void TextToSpeechSource::selectionChanged()
{
  disconnect( ui.volumeSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );
  disconnect( ui.rateSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );

  adjustSliders();

  connect( ui.volumeSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );
  connect( ui.rateSlider, SIGNAL( valueChanged( int ) ), this, SLOT( slidersChanged() ) );
}

VoiceEnginesModel::VoiceEnginesModel( QWidget * parent, Config::VoiceEngines const & voiceEngines ):
  QAbstractItemModel( parent ),
  voiceEngines( voiceEngines )
{
}

void VoiceEnginesModel::removeVoiceEngine( int index )
{
  beginRemoveRows( QModelIndex(), index, index );
  voiceEngines.erase( voiceEngines.begin() + index );
  endRemoveRows();
}

void VoiceEnginesModel::addNewVoiceEngine(
  QString const & engine_name, QLocale locale, QString const & name, QString const & voice_name, int volume, int rate )
{
  if ( engine_name.isEmpty() || name.isEmpty() )
    return;

  Config::VoiceEngine v;
  v.enabled     = true;
  v.engine_name = engine_name;
  v.locale      = locale;
  v.name        = name;
  v.volume      = volume;
  v.rate        = rate;
  v.voice_name  = voice_name;

  beginInsertRows( QModelIndex(), voiceEngines.size(), voiceEngines.size() );
  voiceEngines.push_back( v );
  endInsertRows();
}

QModelIndex VoiceEnginesModel::index( int row, int column, QModelIndex const & /*parent*/ ) const
{
  return createIndex( row, column );
}

QModelIndex VoiceEnginesModel::parent( QModelIndex const & /*parent*/ ) const
{
  return QModelIndex();
}

Qt::ItemFlags VoiceEnginesModel::flags( QModelIndex const & index ) const
{
  Qt::ItemFlags result = QAbstractItemModel::flags( index );

  if ( index.isValid() ) {
    switch ( index.column() ) {
      case kColumnEnabled:
        result |= Qt::ItemIsUserCheckable;
        break;
      case kColumnEngineDName:
      case kColumnIcon:
        result |= Qt::ItemIsEditable;
        break;
    }
  }

  return result;
}

int VoiceEnginesModel::rowCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() )
    return 0;
  return voiceEngines.size();
}

int VoiceEnginesModel::columnCount( QModelIndex const & parent ) const
{
  if ( parent.isValid() )
    return 0;
  return kColumnCount;
}

QVariant VoiceEnginesModel::headerData( int section, Qt::Orientation /*orientation*/, int role ) const
{
  if ( role == Qt::DisplayRole ) {
    switch ( section ) {
      case kColumnEnabled:
        return tr( "Enabled" );
      case kColumnEngineDName:
        return tr( "Name" );
      case kColumnEngineName:
        return tr( "Id" );
      case kColumnIcon:
        return tr( "Icon" );
    }
  }

  return QVariant();
}

QVariant VoiceEnginesModel::data( QModelIndex const & index, int role ) const
{
  if ( index.row() >= voiceEngines.size() )
    return QVariant();

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case kColumnEngineName:
        return voiceEngines[ index.row() ].engine_name;
      case kColumnEngineDName:
        return voiceEngines[ index.row() ].name;
      case kColumnIcon:
        return voiceEngines[ index.row() ].iconFilename;
      default:
        return QVariant();
    }
  }

  if ( role == Qt::CheckStateRole && index.column() == kColumnEnabled )
    return voiceEngines[ index.row() ].enabled ? Qt::Checked : Qt::Unchecked;

  return QVariant();
}

bool VoiceEnginesModel::setData( QModelIndex const & index, const QVariant & value, int role )
{
  if ( index.row() >= voiceEngines.size() )
    return false;

  if ( role == Qt::CheckStateRole && index.column() == kColumnEnabled ) {
    voiceEngines[ index.row() ].enabled = !voiceEngines[ index.row() ].enabled;
    dataChanged( index, index );
    return true;
  }

  if ( role == Qt::DisplayRole || role == Qt::EditRole ) {
    switch ( index.column() ) {
      case kColumnEngineName:
        voiceEngines[ index.row() ].engine_name = value.toString();
        dataChanged( index, index );
        return true;
      case kColumnEngineDName:
        voiceEngines[ index.row() ].name = value.toString();
        dataChanged( index, index );
        return true;
      case kColumnIcon:
        voiceEngines[ index.row() ].iconFilename = value.toString();
        dataChanged( index, index );
        return true;
      default:
        return false;
    }
  }

  return false;
}

void VoiceEnginesModel::setEngineParams( QModelIndex idx, int volume, int rate )
{
  if ( idx.isValid() ) {
    voiceEngines[ idx.row() ].volume = volume;
    voiceEngines[ idx.row() ].rate   = rate;
  }
}

VoiceEngineEditor::VoiceEngineEditor( SpeechClient::Engines const & engines, QWidget * parent ):
  QComboBox( parent )
{
  foreach( SpeechClient::Engine engine, engines )
  {
    addItem( engine.name, engine.engine_name );
  }
}

QString VoiceEngineEditor::engineName() const
{
  int idx = currentIndex();
  if ( idx < 0 )
    return "";
  return itemText( idx );
}

QString VoiceEngineEditor::engineId() const
{
  int idx = currentIndex();
  if ( idx < 0 )
    return "";
  return itemData( idx ).toString();
}

void VoiceEngineEditor::setEngineId( QString const & engineId )
{
  // Find index for the id
  int idx = -1;
  for ( int i = 0; i < count(); ++i ) {
    if ( engineId == itemData( i ).toString() ) {
      idx = i;
      break;
    }
  }
  setCurrentIndex( idx );
}

VoiceEngineItemDelegate::VoiceEngineItemDelegate( SpeechClient::Engines const & engines, QObject * parent ):
  QStyledItemDelegate( parent ),
  engines( engines )
{
}

QWidget * VoiceEngineItemDelegate::createEditor( QWidget * parent,
                                                 QStyleOptionViewItem const & option,
                                                 QModelIndex const & index ) const
{
  if ( index.column() != VoiceEnginesModel::kColumnEngineDName )
    return QStyledItemDelegate::createEditor( parent, option, index );
  return new VoiceEngineEditor( engines, parent );
}

void VoiceEngineItemDelegate::setEditorData( QWidget * uncastedEditor, const QModelIndex & index ) const
{
  VoiceEngineEditor * editor = qobject_cast< VoiceEngineEditor * >( uncastedEditor );
  if ( !editor )
    return;

  int currentRow            = index.row();
  QModelIndex engineIdIndex = index.sibling( currentRow, VoiceEnginesModel::kColumnEngineName );
  QString engineId          = index.model()->data( engineIdIndex ).toString();
  editor->setEngineId( engineId );
}

void VoiceEngineItemDelegate::setModelData( QWidget * uncastedEditor,
                                            QAbstractItemModel * model,
                                            const QModelIndex & index ) const
{
  VoiceEngineEditor * editor = qobject_cast< VoiceEngineEditor * >( uncastedEditor );
  if ( !editor )
    return;

  int currentRow              = index.row();
  QModelIndex engineIdIndex   = index.sibling( currentRow, VoiceEnginesModel::kColumnEngineName );
  QModelIndex engineNameIndex = index.sibling( currentRow, VoiceEnginesModel::kColumnEngineDName );
  model->setData( engineIdIndex, editor->engineId() );
  model->setData( engineNameIndex, editor->engineName() );
}

#endif