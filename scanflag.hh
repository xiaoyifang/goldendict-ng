#ifndef SCAN_FLAG_H
#define SCAN_FLAG_H

#include <QMainWindow>
#include <QPushButton>
#include <QTimer>

class ScanFlag : public QMainWindow
{
  Q_OBJECT

public:
  ScanFlag( QWidget *parent );

  ~ScanFlag()=default;

  void showScanFlag();
  void pushButtonClicked();
  void hideWindow();

signals:
  void requestScanPopup ();

private:
  QTimer hideTimer;
  QPushButton * pushButton;
};

#endif // SCAN_FLAG_H
