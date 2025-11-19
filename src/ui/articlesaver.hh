/* Small helper to centralize article saving logic shared by MainWindow and ScanPopup */
#pragma once

#include <QWidget>
#include <QObject>
#include "ui/articleview.hh"


// Article saver object that performs the same logic as the old free function.
// It emits `statusMessage` for UI code to present to the user (status bars,
// notifications, etc.). The saver itself is a QObject so it can be used with
// Qt's signal/slot mechanism and can be parented for automatic cleanup.
class ArticleSaver: public QObject
{
  Q_OBJECT

public:
  explicit ArticleSaver( QWidget * uiParent, ArticleView * view, Config::Class & cfg );
  ~ArticleSaver() override;

  // Start the save operation. The operation is asynchronous where needed
  // (e.g. resource downloads), but `save()` returns immediately.
  void save();

signals:
  void statusMessage( const QString & text, int timeout );

private:
  QWidget * uiParent_;
  ArticleView * view_;
  Config::Class & cfg_;
};
