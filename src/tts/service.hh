#pragma once

#include <QWidget>
#include <optional>

/*
 *
 * We want maximum decoupling between different services.
 *
 * Things needed by Services should be added to specific services.
 *
 * Consider other options before modifying this file.
 *
 */

namespace TTS {
class Service: public QObject
{
  Q_OBJECT
public slots:
  virtual void speak( QUtf8StringView s ) noexcept {};
  virtual void stop() noexcept {}
signals:
  /// @brief User facing error reporting.
  /// Service::speak is likely async, error cannot be reported at return position.
  void errorOccured( const QString & errorString );
};

class ServiceConfigWidget: public QWidget
{
public:
  explicit ServiceConfigWidget( QWidget * parent ):
    QWidget( parent )
  {
  }

  /// Ask the service to save it's config.
  /// @return if failed, return a string that contains Error message.
  virtual std::optional< std::string > save() noexcept
  {
    return {};
  }
};
} // namespace TTS