#pragma once

#include "tts/service.hh"

#include <QComboBox>
#include <QString>
#include <QtNetwork>
#include <optional>
#include <QMediaPlayer>

namespace Azure {

static const char * azureSaveFileName = "azure.json";

static const char * hostUrlBody = "tts.speech.microsoft.com/cognitiveservices";

struct AzureConfig
{
  QString apiKey;
  QString region;
  QString voiceShortName;

  [[nodiscard]] static std::optional< AzureConfig > loadFromFile( const QString & );
  [[nodiscard]] static bool saveToFile( const QString & configFilePath, const AzureConfig & );
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
  QScopedPointer< QNetworkRequest > voiceListRequest;
  QScopedPointer< QNetworkReply > voiceListReply;

  QComboBox * voiceList;

  void asyncVoiceListPopulating( const QString & autoSelectThisName );
};

class Service: public TTS::Service
{
  Q_OBJECT

public:
  static Service * Construct( const QDir & configRootPath );
  [[nodiscard]] std::optional< std::string > speak( QUtf8StringView s ) noexcept override;

  ~Service() override;

private:
  Service() = default;
  bool private_initalize();
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
} // namespace Azure