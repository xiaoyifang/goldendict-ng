#include "tts/config_window.hh"
#include "tts/services/azure.hh"
#include "tts/services/dummy.hh"
#include "tts/services/local_command.hh"
#include "tts/config_file_main.hh"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

#include <QStringLiteral>

namespace TTS {

//TODO: split preview pane to a seprate file.
void ConfigWindow::setupUi()
{
  setWindowTitle( "Service Config" );
  this->setAttribute( Qt::WA_DeleteOnClose );
  this->setWindowModality( Qt::WindowModal );
  this->setWindowFlag( Qt::Dialog );

  MainLayout = new QGridLayout( this );

  configPane         = new QGroupBox( "Service Config", this );
  auto * previewPane = new QGroupBox( "Audio Preview", this );

  configPane->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
  previewPane->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::MinimumExpanding );

  configPane->setLayout( new QVBoxLayout() );
  previewPane->setLayout( new QVBoxLayout() );

  auto * serviceSelectLayout = new QHBoxLayout( nullptr );
  auto * serviceLabel        = new QLabel( "Select service", this );
  serviceSelector            = new QComboBox();
  serviceSelector->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Maximum );

  serviceSelectLayout->addWidget( serviceLabel );
  serviceSelectLayout->addWidget( serviceSelector );

  previewLineEdit = new QLineEdit( this );
  previewButton   = new QPushButton( "Preview", this );

  previewPane->layout()->addWidget( previewLineEdit );
  previewPane->layout()->addWidget( previewButton );
  qobject_cast< QVBoxLayout * >( previewPane->layout() )->addStretch();

  buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help, this );
  MainLayout->addLayout( serviceSelectLayout, 0, 0, 1, 2 );
  MainLayout->addWidget( configPane, 1, 0, 1, 1 );
  MainLayout->addWidget( previewPane, 1, 1, 1, 1 );
  MainLayout->addWidget( buttonBox, 2, 0, 1, 2 );
  MainLayout->addWidget(
    new QLabel(
      R"(<font color="red">Experimental feature. The default API key may stop working at anytime. Feedback & Coding help are welcomed. </font>)",
      this ),
    3,
    0,
    1,
    2 );
}

ConfigWindow::ConfigWindow( QWidget * parent, const QString & configRootPath ):
  QWidget( parent, Qt::Window ),
  configRootDir( configRootPath )
{
  configRootDir.mkpath( QStringLiteral( "ctts" ) );
  configRootDir.cd( QStringLiteral( "ctts" ) );


  this->setupUi();

  serviceSelector->addItem( "Azure Text to Speech", QStringLiteral( "azure" ) );
  serviceSelector->addItem( "Local Command Line", QStringLiteral( "local_command" ) );
  serviceSelector->addItem( "Dummy", QStringLiteral( "dummy" ) );


  this->currentService = get_service_name_from_path( configRootDir );

  if ( auto i = serviceSelector->findData( this->currentService ); i != -1 ) {
    serviceSelector->setCurrentIndex( i );
  }


  connect( previewButton, &QPushButton::clicked, this, [ this ] {
    this->serviceConfigUI->save();


    if ( currentService == "azure" ) {
      previewService.reset( TTS::AzureService::Construct( this->configRootDir ) );
    }
    else if ( currentService == "local_command" ) {
      auto * s = new TTS::LocalCommandService( this->configRootDir );
      s->loadCommandFromConfigFile(); // TODO:: error unhandled.
      previewService.reset( s );
    }
    else {
      previewService.reset( new TTS::DummyService() );
    }

    if ( previewService != nullptr ) {
      previewService->speak( previewLineEdit->text().toUtf8() );
    }
    else {
      exit( 1 ); // TODO
    }
  } );


  updateConfigPaneBasedOnCurrentService();

  connect( serviceSelector, &QComboBox::currentIndexChanged, this, [ this ] {
    updateConfigPaneBasedOnCurrentService();
  } );

  connect( buttonBox, &QDialogButtonBox::accepted, this, [ this ]() {
    qDebug() << "accept";
    this->serviceConfigUI->save();
    save_service_name_to_path( configRootDir, this->serviceSelector->currentData().toByteArray() );

    emit this->service_changed();
    this->close();
  } );

  connect( buttonBox, &QDialogButtonBox::rejected, this, [ this ]() {
    qDebug() << "rejected";
    this->close();
  } );

  connect( buttonBox->button( QDialogButtonBox::Help ), &QPushButton::clicked, this, [ this ]() {
    qDebug() << "help";
  } );
}


void ConfigWindow::updateConfigPaneBasedOnCurrentService()
{
  if ( serviceSelector->currentData() == "azure" ) {
    serviceConfigUI.reset( new TTS::AzureConfigWidget( this, this->configRootDir ) );
  }
  else if ( serviceSelector->currentData() == "local_command" ) {
    serviceConfigUI.reset( new TTS::LocalCommandConfigWidget( this, this->configRootDir ) );
  }
  else {
    serviceConfigUI.reset( new TTS::DummyConfigWidget( this ) );
  }
  configPane->layout()->addWidget( serviceConfigUI.get() );
}
} // namespace TTS