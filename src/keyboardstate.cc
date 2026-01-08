/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "keyboardstate.hh"

#include <QApplication>

bool KeyboardState::checkModifiersPressed( int mask )
{
  auto modifiers = QApplication::queryKeyboardModifiers();
  return modifiers.testFlags( { Qt::NoModifier | ( mask & Alt ? Qt::AltModifier : Qt::NoModifier )
                                | ( mask & Win ? Qt::MetaModifier : Qt::NoModifier )
                                | ( mask & Ctrl ? Qt::ControlModifier : Qt::NoModifier )
                                | ( mask & Shift ? Qt::ShiftModifier : Qt::NoModifier ) } );
}
