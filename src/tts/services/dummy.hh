#pragma once
#include "tts/service.hh"
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>

namespace TTS {
class DummyService: public TTS::Service
{
  void speak( QUtf8StringView s ) noexcept override
  {
    qDebug() << "dummy speaks" << s;
  };
  void stop() noexcept override
  {
    qDebug() << "dummy stops";
  };
};

class DummyConfigWidget: public TTS::ServiceConfigWidget
{
  Q_OBJECT

public:
  explicit DummyConfigWidget( QWidget * parent ):
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
