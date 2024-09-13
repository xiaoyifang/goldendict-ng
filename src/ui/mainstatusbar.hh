/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef MAINSTATUSBAR_HH
#define MAINSTATUSBAR_HH

#include <QWidget>
#include <QLabel>
#include <QString>
#include <QTimer>

class MainStatusBar: public QWidget
{
  Q_OBJECT

public:
  explicit MainStatusBar( QWidget * parent );
  QString currentMessage() const;

signals:

public slots:
  void showMessage( const QString & text, int timeout = 0, const QPixmap & pixmap = QPixmap() );
  void clearMessage();
  void setBackgroundMessage( QString const & message );

protected:
  void mousePressEvent( QMouseEvent * event ) override;

private:
  // component to display a small picture
  QLabel * picWidget;

  // component to display text
  QLabel * textWidget;

  QTimer * timer;
  QString backgroundMessage;
  QString message;

  bool eventFilter( QObject * obj, QEvent * event ) override;
};

#endif // MAINSTATUSBAR_HH
