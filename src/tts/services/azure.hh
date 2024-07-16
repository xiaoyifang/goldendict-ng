#pragma once

#include "tts/service.hh"

#include <QComboBox>
#include <QString>
#include <QtNetwork>
#include <optional>
#include <QMediaPlayer>

namespace TTS {

static const char * azureSaveFileName = "azure.json";

static const char * hostUrlBody = "tts.speech.microsoft.com/cognitiveservices";



class AzureService: public TTS::Service
{
  Q_OBJECT

public:
  static AzureService * Construct( const QDir & configRootPath );
  void speak( QUtf8StringView s ) noexcept override;
  void stop() noexcept override;

  ~AzureService() override;

private:
  AzureService() = default;
  bool private_initialize();
  QNetworkReply * reply;
  QMediaPlayer * player;
  QNetworkRequest * request;
  QString azureConfigFile;
  std::string voiceShortName;

private slots:
  void slotError( QNetworkReply::NetworkError e );
  void slotSslErrors();

  void mediaErrorOccur( QMediaPlayer::Error error, const QString & errorString );
  void mediaStatus( QMediaPlayer::MediaStatus status );
};

class ConfigWidget: public TTS::ServiceConfigWidget
{
  Q_OBJECT

public:
  explicit ConfigWidget( QWidget * parent, const QDir & configRootPath );

  std::optional< std::string > save() noexcept override;

private:
  QString azureConfigPath;
  QLineEdit * region;
  QLineEdit * apiKey;
  std::unique_ptr< QNetworkRequest > voiceListRequest;
  std::unique_ptr< QNetworkReply > voiceListReply;

  QComboBox * voiceList;

  void asyncVoiceListPopulating( const QString & autoSelectThisName );
};
} // namespace Azure