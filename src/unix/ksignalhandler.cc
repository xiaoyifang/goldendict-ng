/*
    SPDX-FileCopyrightText: 2021 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-3.0-or-later

    Copied from KDE's KCoreAddons with minor modifications
*/
#include <QtGlobal>
#ifdef Q_OS_UNIX

  #include "ksignalhandler.hh"
  #include <QSocketNotifier>
  #include <QTimer>
  #include <QDebug>
  #include <cerrno>
  #include <fcntl.h>
  #include <signal.h>
  #include <sys/socket.h>
  #include <unistd.h>

class KSignalHandlerPrivate: public QObject
{
public:
  static void signalHandler( int signal );
  void handleSignal();

  QSet< int > m_signalsRegistered;
  static int signalFd[ 2 ];
  QSocketNotifier * m_handler = nullptr;

  KSignalHandler * q;
};

int KSignalHandlerPrivate::signalFd[ 2 ];

KSignalHandler::KSignalHandler():
  d( new KSignalHandlerPrivate )
{
  d->q = this;
  if ( ::socketpair( AF_UNIX, SOCK_STREAM, 0, KSignalHandlerPrivate::signalFd ) ) {
    qDebug() << "Couldn't create a socketpair";
    return;
  }

  // ensure the sockets are not leaked to child processes, SOCK_CLOEXEC not supported on macOS
  fcntl( KSignalHandlerPrivate::signalFd[ 0 ], F_SETFD, FD_CLOEXEC );
  fcntl( KSignalHandlerPrivate::signalFd[ 1 ], F_SETFD, FD_CLOEXEC );

  QTimer::singleShot( 0, [ this ] {
    d->m_handler = new QSocketNotifier( KSignalHandlerPrivate::signalFd[ 1 ], QSocketNotifier::Read, this );
    connect( d->m_handler, &QSocketNotifier::activated, d.get(), &KSignalHandlerPrivate::handleSignal );
  } );
}

KSignalHandler::~KSignalHandler()
{
  for ( int sig : std::as_const( d->m_signalsRegistered ) ) {
    signal( sig, nullptr );
  }
  close( KSignalHandlerPrivate::signalFd[ 0 ] );
  close( KSignalHandlerPrivate::signalFd[ 1 ] );
}

void KSignalHandler::watchSignal( int signalToTrack )
{
  d->m_signalsRegistered.insert( signalToTrack );
  signal( signalToTrack, KSignalHandlerPrivate::signalHandler );
}

void KSignalHandlerPrivate::signalHandler( int signal )
{
  const int ret = ::write( signalFd[ 0 ], &signal, sizeof( signal ) );
  if ( ret != sizeof( signal ) ) {
    qDebug() << "signalHandler couldn't write for signal" << strsignal( signal ) << " Got error:" << strerror( errno );
  }
}

void KSignalHandlerPrivate::handleSignal()
{
  m_handler->setEnabled( false );
  int signal;
  const int ret = ::read( KSignalHandlerPrivate::signalFd[ 1 ], &signal, sizeof( signal ) );
  if ( ret != sizeof( signal ) ) {
    qDebug() << "handleSignal couldn't read signal for fd" << KSignalHandlerPrivate::signalFd[ 1 ]
             << " Got error:" << strerror( errno );
    return;
  }
  m_handler->setEnabled( true );

  Q_EMIT q->signalReceived( signal );
}

KSignalHandler * KSignalHandler::self()
{
  static KSignalHandler s_self;
  return &s_self;
}

#endif