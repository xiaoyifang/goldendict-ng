#pragma once
#include "tts/service.hh"
#include <QDir>

namespace TTS {

/// @brief Manage the life time of one single service, which reloaded to other service.
class SingleServiceController: public QObject
{
  Q_OBJECT

public:
  explicit SingleServiceController( const QString & configPath );

public slots:
  void speak( const QString & text );
  void reload(); // TODO handle error


private:
  QDir configRootDir;
  std::unique_ptr< Service > currentService;
};


} // namespace TTS