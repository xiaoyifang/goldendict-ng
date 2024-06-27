#pragma once

#include <QNetworkAccessManager>

/*
 * Notes:
 *
 * This MUST be used only in the main/UI thread
 *
 * Retaining a pointer in objects to this globalNetworkAccessManager is OK.
 * Having a pointer to it reduce minor overhead of global object accessing.
 *
 */

#if ( QT_VERSION < QT_VERSION_CHECK( 6, 4, 0 ) )
Q_GLOBAL_STATIC( QNetworkAccessManager, GlobalNetworkAccessManager );
#else
  #include <qapplicationstatic.h>
Q_APPLICATION_STATIC( QNetworkAccessManager, GlobalNetworkAccessManager );

#endif