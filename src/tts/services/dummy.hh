#pragma once
#include "tts/service.hh"
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>

namespace dummy {
class Service: public TTS::Service
{
  std::optional< std::string > speak( QUtf8StringView s ) noexcept override
  {
    qDebug() << "dummy speaks" << s;
    return {};
  };
};

class ConfigWidget: public TTS::ServiceConfigWidget
{
  Q_OBJECT

public:
  explicit ConfigWidget( QWidget * parent ):
    TTS::ServiceConfigWidget( parent )
  {
    this->setLayout( new QVBoxLayout );
    this->layout()->addWidget(
      new QLabel( R"(This is a sample service. You are welcome to check the source code and add new services. )",
                  parent ) );
  }

  std::optional< std::string > save() noexcept override
  {
    return {};
  };
};

} // namespace dummy
