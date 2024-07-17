#pragma once
#include "tts/service.hh"
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>
#include <QDir>
#include <QProcess>
#include <QLineEdit>

namespace TTS {


class LocalCommandService: public TTS::Service
{
  Q_OBJECT

public:
  explicit LocalCommandService( const QDir & configRootPath );

  /// @brief
  /// @return failure message if any
  std::optional< std::string > loadCommandFromConfigFile();
  ~LocalCommandService();

  void speak( QUtf8StringView s ) noexcept override;
  void stop() noexcept override;
signals:


private:
  QString configFilePath;
  QString command;
  std::unique_ptr< QProcess > process;
};

class LocalCommandConfigWidget: public TTS::ServiceConfigWidget
{
  Q_OBJECT

public:
  explicit LocalCommandConfigWidget( QWidget * parent, const QDir & configRootPath );

  std::optional< std::string > save() noexcept override;

private:
  QLineEdit * commandLineEdit;
  QString configFilePath; // Don't use replace on this
};


} // namespace TTS
