#pragma once
#include <QtGlobal>
#include <QClipboard>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

class BaseClipboardListener: public QObject
{
  Q_OBJECT

public:
  explicit BaseClipboardListener( QObject * parent ):
    QObject( parent ) {};
  virtual QString text()
  {
    return {};
  }
  virtual void stop() {}
  virtual void start() {}

signals:
  void changed( QClipboard::Mode mode );
};
