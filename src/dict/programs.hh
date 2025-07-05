/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QProcess>
#include "dictionary.hh"
#include "config.hh"
#include "text.hh"

/// Support for arbitrary programs.
namespace Programs {

using std::vector;
using std::string;

vector< sptr< Dictionary::Class > > makeDictionaries( const Config::Programs & );

class RunInstance: public QObject
{
  Q_OBJECT
  QProcess process;

public:

  RunInstance();

  // Starts the process. Should only be used once. The finished() signal will
  // be emitted once it finishes. If there's an error, returns false and the
  // description is saved to 'error'.
  bool start( const Config::Program &, const QString & word, QString & error );

signals:
  // Connect to this signal to get run results
  void finished( QByteArray output, QString error );

  // Used internally only
signals:
  void processFinished();
private slots:

  void handleProcessFinished();
};

class ProgramDataRequest: public Dictionary::DataRequest
{
  Q_OBJECT
  Config::Program prg;
  RunInstance instance;

public:

  ProgramDataRequest( const QString & word, const Config::Program & );

  virtual void cancel();

private slots:

  void instanceFinished( QByteArray output, QString error );
};

class ProgramWordSearchRequest: public Dictionary::WordSearchRequest
{
  Q_OBJECT
  Config::Program prg;
  RunInstance instance;

public:

  ProgramWordSearchRequest( const QString & word, const Config::Program & );

  virtual void cancel();

private slots:

  void instanceFinished( QByteArray output, QString error );
};

} // namespace Programs
