/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "translatebox.hh"

#include <QAction>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QStringListModel>
#include <QTimer>
#include <QPainter>

TranslateBox::TranslateBox( QWidget * parent ):
  QWidget( parent ),
  translate_line( new QLineEdit( this ) ),
  m_popupEnabled( false )
{
  completer = new QCompleter( words, this );
  resize( 200, 90 );
  QSizePolicy sizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );
  setSizePolicy( sizePolicy );

  setFocusProxy( translate_line );
  translate_line->setObjectName( "translateLine" );
  translate_line->setTextMargins( 0, 0, 0, 0 );
  translate_line->setFrame( false );
  translate_line->setPlaceholderText( tr( "Type a word or phrase to search dictionaries" ) );

  auto layout = new QHBoxLayout( this );
  setLayout( layout );
  layout->setContentsMargins( 0, 0, 0, 0 );
  layout->addWidget( translate_line );

  dropdown = new QAction( QIcon( ":/icons/1downarrow.svg" ), tr( "Drop-down" ), this );
  connect( dropdown, &QAction::triggered, this, &TranslateBox::rightButtonClicked );

  translate_line->addAction( dropdown, QLineEdit::TrailingPosition );
  translate_line->addAction( new QAction( QIcon( ":/icons/system-search.svg" ), "", this ),
                             QLineEdit::LeadingPosition );

  translate_line->setFocusPolicy( Qt::ClickFocus );

  translate_line->installEventFilter( this );

  translate_line->setCompleter( completer );
  completer->setCompletionMode( QCompleter::UnfilteredPopupCompletion );
  completer->setMaxVisibleItems( 16 );
  completer->popup()->setMinimumHeight( 256 );

  // QCompleter's popup comes with the Qt::Popup window flag, which grabs all key
  // events on Wayland and breaks input methods while the popup is open (#3123).
  // Use tooltip-like flags on non-xcb platforms instead, as Falkon's
  // LocationCompleterView and KDE's KCompletionBox do: on Wayland it maps to an
  // xdg_popup without a grab, so the keyboard focus stays on the line edit and
  // IME processing keeps working.
  if ( QApplication::platformName() != QLatin1String( "xcb" ) ) {
    completer->popup()->setWindowFlags( Qt::ToolTip | Qt::FramelessWindowHint | Qt::BypassWindowManagerHint );
  }

  // Qt::Popup used to close the popup automatically on outside clicks; without
  // it, hide the popup when the focus moves away from the line edit.
  connect( qApp, &QApplication::focusChanged, this, [ this ]( QWidget * old, QWidget * now ) {
    if ( old == translate_line && now != translate_line ) {
      completer->popup()->hide();
    }
  } );

  connect( translate_line, &QLineEdit::returnPressed, this, [ this ]() {
    emit returnPressed();
  } );
}

void TranslateBox::setText( const QString & text, bool showPopup )
{
  setPopupEnabled( showPopup );
  translate_line->setText( text );
}

void TranslateBox::setPopupEnabled( bool enable )
{
  m_popupEnabled = enable;
  showPopup();
}

void TranslateBox::setSizePolicy( QSizePolicy policy )
{
  QWidget::setSizePolicy( policy );
  if ( translate_line ) {
    translate_line->setSizePolicy( policy );
  }
}

void TranslateBox::setModel( QStringList & _words )
{
  disconnect( completer, QOverload< const QString & >::of( &QCompleter::activated ), translate_line, nullptr );
  const auto model = static_cast< QStringListModel * >( completer->model() );

  model->setStringList( _words );

  completer->popup()->scrollToTop();

  connect( completer,
           QOverload< const QString & >::of( &QCompleter::activated ),
           translate_line,
           [ & ]( const QString & text ) {
             translate_line->setText( text );
             emit returnPressed();
           } );
}

void TranslateBox::setNoResults( bool noResults )
{
  if ( noResults ) {
    QIcon icon( ":/icons/1downarrow.svg" );
    QPixmap pixmap = icon.pixmap( 32, 32 );
    QPainter painter( &pixmap );
    painter.setCompositionMode( QPainter::CompositionMode_SourceIn );
    painter.fillRect( pixmap.rect(), QColor( 255, 0, 0, 200 ) );
    painter.end();
    dropdown->setIcon( QIcon( pixmap ) );
  }
  else {
    dropdown->setIcon( QIcon( ":/icons/1downarrow.svg" ) );
  }
}

void TranslateBox::showPopup()
{
  if ( m_popupEnabled ) {
    completer->popup()->show();
    completer->complete();
  }
  else {
    completer->popup()->hide();
  }
}

bool TranslateBox::eventFilter( QObject * watched, QEvent * event )
{
  if ( watched == translate_line && event->type() == QEvent::KeyPress ) {
    auto keyEvent = static_cast< QKeyEvent * >( event );
    QWidget * popup = completer->popup();

    // The Qt::Popup keyboard grab used to route key events through the popup, where
    // QCompleter::eventFilter handled the navigation. Without the grab the events
    // arrive at the line edit directly, so the navigation keys are re-routed to the
    // popup. Other keys (typing, Enter) stay with the line edit, which also keeps
    // the input method working while the popup is open.
    switch ( keyEvent->key() ) {
    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
      if ( popup->isVisible() ) {
        QApplication::sendEvent( popup, event );
        return true;
      }
      break;
    case Qt::Key_Escape:
      if ( popup->isVisible() ) {
        popup->hide();
        return true;
      }
      break;
    default:
      break;
    }
  }
  return QObject::eventFilter( watched, event );
}

QLineEdit * TranslateBox::translateLine()
{
  return translate_line;
}

QWidget * TranslateBox::completerWidget()
{
  return completer->popup();
}

void TranslateBox::rightButtonClicked()
{
  setPopupEnabled( !m_popupEnabled );
}
void TranslateBox::setSizePolicy( QSizePolicy::Policy hor, QSizePolicy::Policy ver )
{
  setSizePolicy( QSizePolicy( hor, ver ) );
}
