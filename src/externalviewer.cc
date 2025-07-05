/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <QDir>
#include <QTimer>
#include "externalviewer.hh"

ExternalViewer::ExternalViewer(
  const char * data, int size, const QString & extension, const QString & viewerCmdLine_, QObject * parent ):
  QObject( parent ),
  tempFile( QDir::temp().filePath( QString( "gd-XXXXXXXX." ) + extension ) ),
  viewer( this ),
  viewerCmdLine( viewerCmdLine_ )
{
  if ( !tempFile.open() || tempFile.write( data, size ) != size ) {
    throw exCantCreateTempFile();
  }

  tempFileName = tempFile.fileName(); // For some reason it loses it after it was closed()

#ifdef Q_OS_WIN32
  // Issue #239
  tempFileName = QDir::toNativeSeparators( tempFileName );
#endif

  tempFile.close();

  qDebug( "%s", tempFile.fileName().toLocal8Bit().data() );
}

void ExternalViewer::start()
{
  connect( &viewer, &QProcess::finished, this, &QObject::deleteLater );
  connect( &viewer, &QProcess::errorOccurred, this, &QObject::deleteLater );

  QStringList args = QProcess::splitCommand( viewerCmdLine );
  if ( !args.isEmpty() ) {
    const QString program = args.takeFirst();
    args.push_back( tempFileName );
    viewer.start( program, args, QIODevice::NotOpen );
    if ( !viewer.waitForStarted() ) {
      throw exCantRunViewer( viewerCmdLine.toUtf8().data() );
    }
  }
  else {
    throw exCantRunViewer( tr( "the viewer program name is empty" ).toUtf8().data() );
  }
}

bool ExternalViewer::stop()
{
  if ( viewer.state() == QProcess::NotRunning ) {
    return true;
  }
  viewer.terminate();
  QTimer::singleShot( 1000, &viewer, &QProcess::kill ); // In case terminate() fails.
  return false;
}

void ExternalViewer::stopSynchronously()
{
  // This implementation comes straight from QProcess::~QProcess().
  if ( viewer.state() == QProcess::NotRunning ) {
    return;
  }
  viewer.kill();
  viewer.waitForFinished();
}

void stopAndDestroySynchronously( ExternalViewer * viewer )
{
  if ( !viewer ) {
    return;
  }
  viewer->stopSynchronously();
  delete viewer;
}
