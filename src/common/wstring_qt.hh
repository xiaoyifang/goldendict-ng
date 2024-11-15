/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

/// This file adds conversions between gd::wstring and QString. See wstring.hh
/// for more details on gd::wstring.

#include "wstring.hh"
#include <QString>

namespace gd {
wstring removeTrailingZero( wstring const & v );
wstring removeTrailingZero( QString const & in );
wstring normalize( wstring const & );
} // namespace gd
