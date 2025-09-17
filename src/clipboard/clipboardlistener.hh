#pragma once
#include "clipboard/base.hh"

namespace clipboardListener {
BaseClipboardListener * get_impl( QObject * parent );
}
