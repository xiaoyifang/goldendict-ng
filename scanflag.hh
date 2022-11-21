#ifndef SCAN_FLAG_H
#define SCAN_FLAG_H


#include <QMainWindow>
#include <QTimer>
#include "ui_scanflag.h"

class ScanFlag : public QMainWindow
{
  Q_OBJECT

public:
  ScanFlag( QWidget *parent );

  ~ScanFlag();

  void showScanFlag();
  void pushButtonClicked();
  void hideWindow();

signals:
  void requestScanPopup ();

private:
  Ui::ScanFlag ui;
  QTimer hideTimer;

};

#endif // SCAN_FLAG_H
