#pragma once

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QVBoxLayout>

class ArticleInspector: public QWidget
{
  Q_OBJECT
  QWebEngineView * viewContainer = nullptr;

public:
  ArticleInspector( QWidget * parent = nullptr );

  void setInspectPage( QWebEnginePage * page );
  void triggerAction( QWebEnginePage * page );

private:

  virtual void closeEvent( QCloseEvent * );
};
