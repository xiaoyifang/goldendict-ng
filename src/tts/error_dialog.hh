#pragma once

#include <QMessageBox>

namespace TTS {
void reportError( const QString & errorString )
{
  QMessageBox msgBox{};
  msgBox.setText( "Text to speech failed: " % errorString );
  msgBox.setIcon( QMessageBox::Warning );
  msgBox.exec();
}
} // namespace TTS