#pragma once
#include <QDir>

namespace TTS {
QString get_service_name_from_path( const QDir & configRootPath );
void save_service_name_to_path( const QDir & configPath, QUtf8StringView serviceName );
} // namespace TTS