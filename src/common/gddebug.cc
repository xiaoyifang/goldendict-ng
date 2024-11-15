/* This file is (c) 2013 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "gddebug.hh"
#include <QDebug>
#include <QString>
#include <QtCore5Compat/QTextCodec>

QFile * logFilePtr;

void gdWarning( const char * msg, ... )
{
  va_list ap;
  va_start( ap, msg );

  qWarning() << QString().vasprintf( msg, ap );

  va_end( ap );
}

void gdDebug( const char * msg, ... )
{
  va_list ap;
  va_start( ap, msg );

  qDebug().noquote() << QString().vasprintf( msg, ap );

  va_end( ap );
}
