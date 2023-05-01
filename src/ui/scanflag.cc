#include "scanflag.hh"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>


ScanFlag::ScanFlag(QWidget *parent) :
    QMainWindow(parent),
    pushButton(new QPushButton(this))
{

  pushButton->setIcon(QIcon(":/icons/programicon.png"));

  setCentralWidget(pushButton);

  setFixedSize(30,30);

  setWindowFlags( Qt::ToolTip
                | Qt::FramelessWindowHint
                | Qt::WindowStaysOnTopHint
                | Qt::WindowDoesNotAcceptFocus);


  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_X11DoNotAcceptFocus);

  hideTimer.setSingleShot( true );
  hideTimer.setInterval( 1000 );

  connect( &hideTimer, &QTimer::timeout,this,&ScanFlag::hideWindow);
  connect( pushButton, &QPushButton::clicked,
           this, &ScanFlag::pushButtonClicked );
}

void ScanFlag::pushButtonClicked()
{
  hideTimer.stop();
  hide();
  emit requestScanPopup();
}

void ScanFlag::hideWindow()
{
  if ( isVisible() )
    hide();
}

void ScanFlag::showScanFlag()
{
  if ( isVisible() )
    hide();

  QPoint currentPos = QCursor::pos();

  QRect desktop = QGuiApplication::primaryScreen()->geometry();

  QSize windowSize = geometry().size();

  int x, y;

  /// Try the to-the-right placement
  if ( currentPos.x() + 4 + windowSize.width() <= desktop.topRight().x() )
    x = currentPos.x() + 4;
  else
  /// Try the to-the-left placement
  if ( currentPos.x() - 4 - windowSize.width() >= desktop.x() )
    x = currentPos.x() - 4 - windowSize.width();
  else
  // Center it
    x = desktop.x() + ( desktop.width() - windowSize.width() ) / 2;

  /// Try the to-the-top placement
  if ( currentPos.y() - 15 - windowSize.height() >= desktop.y() )
    y = currentPos.y() - 15 - windowSize.height();
  else
  /// Try the to-the-bottom placement
  if ( currentPos.y() + 15 + windowSize.height() <= desktop.bottomLeft().y() )
    y = currentPos.y() + 15;
  else
  // Center it
    y = desktop.y() + ( desktop.height() - windowSize.height() ) / 2;

  move( x, y );

  show();
  hideTimer.start();
}
