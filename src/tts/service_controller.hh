#pragma once
#include "tts/service.hh"
#include <QDir>

namespace TTS {

/// or call this ServiceManager?
class ServiceController: public QObject
{
  Q_OBJECT

public:
  explicit ServiceController( const QString & configPath );

public slots:
  void speak( const QString & text );
  void reload(); // TODO handle error


private:
  QDir configRootDir;
  std::unique_ptr< Service > currentService;
};


} // namespace TTS