/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "keyboardstate.hh"

#include <QApplication>

bool KeyboardState::checkModifiersPressed( int mask )
{
  auto modifiers = QApplication::queryKeyboardModifiers();

  return !( ( ((mask & Alt) != 0) && !( modifiers.testFlag( Qt::AltModifier ) ) )
            || ( ((mask & Ctrl) != 0) && !( modifiers.testFlag( Qt::ControlModifier ) ) )
            || ( ((mask & Shift) != 0) && !( modifiers.testFlag( Qt::ShiftModifier ) ) ) );
}
