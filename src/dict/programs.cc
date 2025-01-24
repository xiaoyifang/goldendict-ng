/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "programs.hh"
#include "audiolink.hh"
#include "htmlescape.hh"
#include "text.hh"
#include "iconv.hh"
#include "utils.hh"
#include "globalbroadcaster.hh"

#include <QDir>
#include <QFileInfo>

namespace Programs {

using namespace Dictionary;

namespace {

class ProgramsDictionary: public Dictionary::Class
{
  Config::Program prg;

public:

  ProgramsDictionary( Config::Program const & prg_ ):
    Dictionary::Class( prg_.id.toStdString(), vector< string >() ),
    prg( prg_ )
  {
  }

  string getName() noexcept override
  {
    return prg.name.toUtf8().data();
  }

  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( std::u32string const & word, unsigned long maxResults ) override;

  sptr< DataRequest >
  getArticle( std::u32string const &, vector< std::u32string > const & alts, std::u32string const &, bool ) override;

protected:

  void loadIcon() noexcept override;
};

sptr< WordSearchRequest > ProgramsDictionary::prefixMatch( std::u32string const & word, unsigned long /*maxResults*/ )

{
  if ( prg.type == Config::Program::PrefixMatch ) {
    return std::make_shared< ProgramWordSearchRequest >( QString::fromStdU32String( word ), prg );
  }
  else {
    sptr< WordSearchRequestInstant > sr = std::make_shared< WordSearchRequestInstant >();

    sr->setUncertain( true );

    return sr;
  }
}

sptr< Dictionary::DataRequest > ProgramsDictionary::getArticle( std::u32string const & word,
                                                                vector< std::u32string > const &,
                                                                std::u32string const &,
                                                                bool )

{
  switch ( prg.type ) {
    case Config::Program::Audio: {
      // Audio results are instantaneous
      string result;

      string wordUtf8( Text::toUtf8( word ) );

      result += "<div class=\"audio-play\"><div class=\"audio-play-item\">";

      QUrl url;
      url.setScheme( "gdprg" );
      url.setHost( QString::fromUtf8( getId().c_str() ) );
      url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( wordUtf8.c_str() ) ) );

      string ref = string( "\"" ) + url.toEncoded().data() + "\"";

      addAudioLink( url.toEncoded(), getId() );

      result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
      result += "<a href=" + ref + ">" + Html::escape( wordUtf8 ) + "</a>";
      result += "</div></div>";

      auto ret = std::make_shared< DataRequestInstant >( true );
      ret->appendString( result );
      return ret;
    }

    case Config::Program::Html:
    case Config::Program::PlainText:
      return std::make_shared< ProgramDataRequest >( QString::fromStdU32String( word ), prg );

    default:
      return std::make_shared< DataRequestInstant >( false );
  }
}

void ProgramsDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !prg.iconFilename.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), prg.iconFilename );
    if ( fInfo.isFile() ) {
      loadIconFromFile( fInfo.absoluteFilePath(), true );
    }
  }
  if ( dictionaryIcon.isNull() ) {
    dictionaryIcon = QIcon( ":/icons/programdict.svg" );
  }
  dictionaryIconLoaded = true;
}

} // namespace

RunInstance::RunInstance():
  process( this )
{
  connect( this, &RunInstance::processFinished, this, &RunInstance::handleProcessFinished, Qt::QueuedConnection );
  connect( &process, &QProcess::finished, this, &RunInstance::processFinished );
  connect( &process, &QProcess::errorOccurred, this, &RunInstance::processFinished );
}

bool RunInstance::start( Config::Program const & prg, QString const & word, QString & error )
{
  QStringList args = QProcess::splitCommand( prg.commandLine );

  if ( !args.empty() ) {
    QString programName = args.first();
    args.pop_front();

    bool writeToStdInput       = true;
    auto const & search_string = GlobalBroadcaster::instance()->translateLineText;

    for ( auto & arg : args ) {
      if ( arg.indexOf( "%GDWORD%" ) >= 0 ) {
        writeToStdInput = false;
        arg.replace( "%GDWORD%", word );
      }
      if ( arg.indexOf( "%GDSEARCH%" ) >= 0 ) {
        writeToStdInput = false;
        arg.replace( "%GDSEARCH%", search_string );
      }
    }

    process.start( programName, args );
    if ( writeToStdInput ) {
      process.write( word.toUtf8() );
      process.closeWriteChannel();
    }

    return true;
  }
  else {
    error = tr( "No program name was given." );
    return false;
  }
}

void RunInstance::handleProcessFinished()
{
  // It seems that sometimes the process isn't finished yet despite being
  // signalled as such. So we wait for it here, which should hopefully be
  // nearly instant.
  process.waitForFinished();

  QByteArray output = process.readAllStandardOutput();

  QString error;
  if ( process.exitStatus() != QProcess::NormalExit ) {
    error = tr( "The program has crashed." );
  }
  else if ( int code = process.exitCode() ) {
    error = tr( "The program has returned exit code %1." ).arg( code );
  }

  if ( !error.isEmpty() ) {
    QByteArray err = process.readAllStandardError();

    if ( !err.isEmpty() ) {
      error += "\n\n" + QString::fromUtf8( err );
    }
  }

  emit finished( output, error );
}

ProgramDataRequest::ProgramDataRequest( QString const & word, Config::Program const & prg_ ):
  prg( prg_ )
{
  connect( &instance, &RunInstance::finished, this, &ProgramDataRequest::instanceFinished );

  QString error;
  if ( !instance.start( prg, word, error ) ) {
    setErrorString( error );
    finish();
  }
}

void ProgramDataRequest::instanceFinished( QByteArray output, QString error )
{
  QString prog_output;
  if ( !isFinished() ) {
    if ( !output.isEmpty() ) {
      string result = "<div class='programs_";

      switch ( prg.type ) {
        case Config::Program::PlainText:
          result += "plaintext'>";
          try {
            // Check BOM if present
            unsigned char * uchars = reinterpret_cast< unsigned char * >( output.data() );
            if ( output.length() >= 2 && uchars[ 0 ] == 0xFF && uchars[ 1 ] == 0xFE ) {
              int size = output.length() - 2;
              if ( size & 1 ) {
                size -= 1;
              }
              string res  = Iconv::toUtf8( "UTF-16LE", output.data() + 2, size );
              prog_output = QString::fromUtf8( res.c_str(), res.size() );
            }
            else if ( output.length() >= 2 && uchars[ 0 ] == 0xFE && uchars[ 1 ] == 0xFF ) {
              int size = output.length() - 2;
              if ( size & 1 ) {
                size -= 1;
              }
              string res  = Iconv::toUtf8( "UTF-16BE", output.data() + 2, size );
              prog_output = QString::fromUtf8( res.c_str(), res.size() );
            }
            else if ( output.length() >= 3 && uchars[ 0 ] == 0xEF && uchars[ 1 ] == 0xBB && uchars[ 2 ] == 0xBF ) {
              prog_output = QString::fromUtf8( output.data() + 3, output.length() - 3 );
            }
            else {
              // No BOM, assume UTF-8 encoding
              prog_output = QString::fromUtf8( output );
            }
          }
          catch ( std::exception & e ) {
            error = e.what();
          }
          result += Html::preformat( prog_output.toUtf8().data() );
          break;
        default:
          result += "html'>";
          try {
            // Check BOM if present
            unsigned char * uchars = reinterpret_cast< unsigned char * >( output.data() );
            if ( output.length() >= 2 && uchars[ 0 ] == 0xFF && uchars[ 1 ] == 0xFE ) {
              int size = output.length() - 2;
              if ( size & 1 ) {
                size -= 1;
              }
              result += Iconv::toUtf8( "UTF-16LE", output.data() + 2, size );
            }
            else if ( output.length() >= 2 && uchars[ 0 ] == 0xFE && uchars[ 1 ] == 0xFF ) {
              int size = output.length() - 2;
              if ( size & 1 ) {
                size -= 1;
              }
              result += Iconv::toUtf8( "UTF-16BE", output.data() + 2, size );
            }
            else if ( output.length() >= 3 && uchars[ 0 ] == 0xEF && uchars[ 1 ] == 0xBB && uchars[ 2 ] == 0xBF ) {
              result += output.data() + 3;
            }
            else {
              // We assume html data is in utf8 encoding already.
              result += output.data();
            }
          }
          catch ( std::exception & e ) {
            error = e.what();
          }
      }

      result += "</div>";

      QMutexLocker _( &dataMutex );
      data.resize( result.size() );
      memcpy( data.data(), result.data(), data.size() );
      hasAnyData = true;
    }

    if ( !error.isEmpty() ) {
      setErrorString( error );
    }

    finish();
  }
}

void ProgramDataRequest::cancel()
{
  finish();
}

ProgramWordSearchRequest::ProgramWordSearchRequest( QString const & word, Config::Program const & prg_ ):
  prg( prg_ )
{
  connect( &instance, &RunInstance::finished, this, &ProgramWordSearchRequest::instanceFinished );

  QString error;
  if ( !instance.start( prg, word, error ) ) {
    setErrorString( error );
    finish();
  }
}

void ProgramWordSearchRequest::instanceFinished( QByteArray output, QString error )
{
  if ( !isFinished() ) {
    // Handle any Windows artifacts
    output.replace( "\r\n", "\n" );
    QStringList result = QString::fromUtf8( output ).split( "\n", Qt::SkipEmptyParts );

    for ( const auto & x : result ) {
      matches.push_back( Dictionary::WordMatch( x.toStdU32String() ) );
    }

    if ( !error.isEmpty() ) {
      setErrorString( error );
    }

    finish();
  }
}

void ProgramWordSearchRequest::cancel()
{
  finish();
}

vector< sptr< Dictionary::Class > > makeDictionaries( Config::Programs const & programs )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & program : programs ) {
    if ( program.enabled ) {
      result.push_back( std::make_shared< ProgramsDictionary >( program ) );
    }
  }

  return result;
}

} // namespace Programs
