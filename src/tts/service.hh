#pragma once

#include <QWidget>
#include <optional>

/*
 *
 * Do not add anything new to this header to ensure maximum decouping between different services.
 *
 * Things needed by Services, should be added to specific services.
 *
 * If something is needed by multiple services,
 * it should be implemented as a component that
 * can be plugged into needed services.
 *
 * Consider other options before modifying this file.
 *
 */

namespace TTS {
class Service: public QObject
{
  Q_OBJECT
public slots:
  ///
  /// @return If failed, return a string that contains Error message.
  [[nodiscard]] virtual std::optional< std::string > speak( QUtf8StringView s ) noexcept
  {
    return {};
  }
  virtual void stop() noexcept {} // TODO: does here need error handling?
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