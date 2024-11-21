/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

// Installs the termination handler which attempts to pop Qt's dialog showing
// the exception and backtrace, and then aborts.
void installTerminationHandler();
