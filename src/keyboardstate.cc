/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "keyboardstate.hh"
#include <QObject> // To get Qt Q_OS defines

#include <QApplication>

bool KeyboardState::checkModifiersPressed( int mask )
{

  auto modifiers = QApplication::keyboardModifiers();

  qDebug() << modifiers;

  return !( ( mask & Alt && !( modifiers.testFlag( Qt::AltModifier ) ) )
            || ( mask & Ctrl && !( modifiers.testFlag( Qt::ControlModifier ) ) )
            || ( mask & Shift && !( modifiers.testFlag( Qt::ShiftModifier ) ) )
            || ( mask & LeftAlt && !( modifiers.testFlag( Qt::AltModifier ) ) )
            || ( mask & RightAlt && !( modifiers.testFlag( Qt::AltModifier ) ) )
            || ( mask & LeftCtrl && !( modifiers.testFlag( Qt::ControlModifier ) ) )
            || ( mask & RightCtrl && !( modifiers.testFlag( Qt::ControlModifier ) ) )
            || ( mask & LeftShift && !( modifiers.testFlag( Qt::ShiftModifier ) ) )
            || ( mask & RightShift && !( modifiers.testFlag( Qt::ShiftModifier ) ) ) );
}
