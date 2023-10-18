/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictserver.hh"
#include "wstring_qt.hh"
#include <QUrl>
#include <QTcpSocket>
#include <QString>
#include <list>
#include "gddebug.hh"
#include "htmlescape.hh"

#include <QRegularExpression>
#include <QtConcurrent>

namespace DictServer {

using namespace Dictionary;

enum {
  DefaultPort = 2628
};

namespace {

#define MAX_MATCHES_COUNT 60

bool readLine( QTcpSocket & socket, QString & line, QString & errorString, QAtomicInt & isCancelled )
{
  line.clear();
  errorString.clear();
  if ( socket.state() != QTcpSocket::ConnectedState )
    return false;

  for ( ;; ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return false;

    if ( socket.canReadLine() ) {
      QByteArray reply = socket.readLine();
      line             = QString::fromUtf8( reply.data(), reply.size() );
      return true;
    }

    if ( !socket.waitForReadyRead( 2000 ) ) {
      errorString =
        "Data reading error: socket error " + QString::number( socket.error() ) + ": \"" + socket.errorString() + "\"";
      break;
    }
  }
  return false;
}

bool connectToServer( QTcpSocket & socket, QString const & url, QString & errorString, QAtomicInt & isCancelled )
{
  QUrl serverUrl( url );
  quint16 port = serverUrl.port( DefaultPort );
  QString reply;

  for ( ;; ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return false;

    socket.connectToHost( serverUrl.host(), port );
    if ( socket.state() != QTcpSocket::ConnectedState ) {
      if ( !socket.waitForConnected( 1500 ) )
        break;
    }

    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return false;

    if ( !readLine( socket, reply, errorString, isCancelled ) )
      break;

    if ( !reply.isEmpty() && reply.left( 3 ) != "220" ) {
      errorString = "Server refuse connection: " + reply;
      return false;
    }

    QString msgId = reply.mid( reply.lastIndexOf( " " ) ).trimmed();

    socket.write( QByteArray( "CLIENT GoldenDict\r\n" ) );
    if ( !socket.waitForBytesWritten( 1000 ) )
      break;

    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return false;

    if ( !readLine( socket, reply, errorString, isCancelled ) )
      break;

    if ( !serverUrl.userInfo().isEmpty() ) {
      QString authCommand = QString( "AUTH " );
      QString authString  = msgId;

      int pos = serverUrl.userInfo().indexOf( QRegularExpression( "[:;]" ) );
      if ( pos > 0 ) {
        authCommand += serverUrl.userInfo().left( pos );
        authString += serverUrl.userInfo().mid( pos + 1 );
      }
      else
        authCommand += serverUrl.userInfo();

      authCommand += " ";
      authCommand += QCryptographicHash::hash( authString.toUtf8(), QCryptographicHash::Md5 ).toHex();
      authCommand += "\r\n";

      socket.write( authCommand.toUtf8() );

      if ( !socket.waitForBytesWritten( 1000 ) )
        break;

      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        return false;

      if ( !readLine( socket, reply, errorString, isCancelled ) )
        break;

      if ( reply.left( 3 ) != "230" ) {
        errorString = "Authentication error: " + reply;
        return false;
      }
    }

    socket.write( QByteArray( "OPTION MIME\r\n" ) );

    if ( !socket.waitForBytesWritten( 1000 ) )
      break;

    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      return false;

    if ( !readLine( socket, reply, errorString, isCancelled ) )
      break;

    if ( reply.left( 3 ) != "250" ) {
      // RFC 2229, 3.10.1.1:
      // OPTION MIME is a REQUIRED server capability,
      // all DICT servers MUST implement this command.
      errorString = "Server doesn't support mime capability: " + reply;
      return false;
    }

    return true;
  }

  if ( !Utils::AtomicInt::loadAcquire( isCancelled ) )
    errorString = QString( "Server connection fault, socket error %1: \"%2\"" )
                    .arg( QString::number( socket.error() ) )
                    .arg( socket.errorString() );
  return false;
}

void disconnectFromServer( QTcpSocket & socket )
{
  if ( socket.state() == QTcpSocket::ConnectedState )
    socket.write( QByteArray( "QUIT\r\n" ) );

  socket.disconnectFromHost();
}

class DictServerDictionary: public Dictionary::Class
{
  string name;
  QString url, icon;
  quint32 langId;
  QString errorString;
  QTcpSocket socket;
  QStringList databases;
  QStringList strategies;
  QStringList serverDatabases;

public:

  DictServerDictionary( string const & id,
                        string const & name_,
                        QString const & url_,
                        QString const & database_,
                        QString const & strategies_,
                        QString const & icon_ ):
    Dictionary::Class( id, vector< string >() ),
    name( name_ ),
    url( url_ ),
    icon( icon_ ),
    langId( 0 )
  {
    int pos = url.indexOf( "://" );
    if ( pos < 0 )
      url = "dict://" + url;

    databases = database_.split( QRegularExpression( "[ ,;]" ), Qt::SkipEmptyParts );
    if ( databases.isEmpty() )
      databases.append( "*" );

    strategies = strategies_.split( QRegularExpression( "[ ,;]" ), Qt::SkipEmptyParts );
    if ( strategies.isEmpty() )
      strategies.append( "prefix" );
    QUrl serverUrl( url );
    quint16 port = serverUrl.port( DefaultPort );
    QString reply;
    socket.connectToHost( serverUrl.host(), port );
    connect( &socket, &QTcpSocket::connected, this, [ this ]() {
      //initialize the description.
      getServerDatabasesAfterConnect();
    } );
    connect( &socket, &QTcpSocket::stateChanged, this, []( QAbstractSocket::SocketState state ) {
      qDebug() << "socket state change: " << state;
    } );
    connect( &socket, &QTcpSocket::errorOccurred, this, []( QAbstractSocket::SocketError error ) {
      qDebug() << "socket error message: " << error;
    } );
  }

  ~DictServerDictionary() override
  {
    disconnectFromServer( socket );
  }

  string getName() noexcept override
  {
    return name;
  }

  map< Property, string > getProperties() noexcept override
  {
    return {};
  }

  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( wstring const &, unsigned long maxResults ) override;

  sptr< DataRequest > getArticle( wstring const &, vector< wstring > const & alts, wstring const &, bool ) override;

  quint32 getLangFrom() const override
  {
    return langId;
  }

  quint32 getLangTo() const override
  {
    return langId;
  }

  QString const & getDescription() override;

protected:

  void loadIcon() noexcept override;

  friend class DictServerWordSearchRequest;
  friend class DictServerArticleRequest;
  void getServerDatabasesAfterConnect();
};

void DictServerDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  if ( !icon.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), icon );
    if ( fInfo.isFile() )
      loadIconFromFile( fInfo.absoluteFilePath(), true );
  }
  if ( dictionaryIcon.isNull() )
    dictionaryIcon = QIcon( ":/icons/network.svg" );
  dictionaryIconLoaded = true;
}

QString const & DictServerDictionary::getDescription()
{
  if ( dictionaryDescription.isEmpty() ) {
    dictionaryDescription = QCoreApplication::translate( "DictServer", "Url: " ) + url + "\n";
    dictionaryDescription += QCoreApplication::translate( "DictServer", "Databases: " ) + databases.join( ", " ) + "\n";
    dictionaryDescription +=
      QCoreApplication::translate( "DictServer", "Search strategies: " ) + strategies.join( ", " );
    if ( !serverDatabases.isEmpty() ) {
      dictionaryDescription += "\n\n";
      dictionaryDescription += QCoreApplication::translate( "DictServer", "Server databases" ) + " ("
        + QString::number( serverDatabases.size() ) + "):";
      for ( const auto & serverDatabase : serverDatabases )
        dictionaryDescription += "\n" + serverDatabase;
    }
  }
  return dictionaryDescription;
}

void DictServerDictionary::getServerDatabasesAfterConnect()
{
  QAtomicInt isCancelled;

  for ( ;; ) {
    QString req = QString( "SHOW DB\r\n" );
    socket.write( req.toUtf8() );
    socket.waitForBytesWritten( 1000 );

    QString reply;

    if ( !readLine( socket, reply, errorString, isCancelled ) )
      return;

    if ( reply.left( 3 ) == "110" ) {
      int countPos = reply.indexOf( ' ', 4 );
      // Get databases count
      int count = reply.mid( 4, countPos > 4 ? countPos - 4 : -1 ).toInt();

      // Read databases
      for ( int x = 0; x < count; x++ ) {
        if ( !readLine( socket, reply, errorString, isCancelled ) )
          break;

        if ( reply[ 0 ] == '.' )
          break;

        while ( reply.endsWith( '\r' ) || reply.endsWith( '\n' ) )
          reply.chop( 1 );

        if ( !reply.isEmpty() )
          serverDatabases.append( reply );
      }

      break;
    }
    else {
      gdWarning( "Retrieving databases from \"%s\" fault: %s\n", getName().c_str(), reply.toUtf8().data() );
      break;
    }
  }
}

class DictServerWordSearchRequest: public Dictionary::WordSearchRequest
{
  QAtomicInt isCancelled;
  wstring word;
  QString errorString;
  QFuture< void > f;
  DictServerDictionary & dict;
  QTcpSocket * socket = nullptr;

public:

  DictServerWordSearchRequest( wstring const & word_, DictServerDictionary & dict_ ):
    word( word_ ),
    dict( dict_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  ~DictServerWordSearchRequest() override
  {
    f.waitForFinished();
  }

  void cancel() override;
};

void DictServerWordSearchRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  socket = new QTcpSocket;

  if ( !socket ) {
    finish();
    return;
  }

  if ( connectToServer( *socket, dict.url, errorString, isCancelled ) ) {
    QStringList matchesList;

    for ( int ns = 0; ns < dict.strategies.size(); ns++ ) {
      for ( int i = 0; i < dict.databases.size(); i++ ) {
        QString matchReq = QString( "MATCH " ) + dict.databases.at( i ) + " " + dict.strategies.at( ns ) + " \""
          + QString::fromStdU32String( word ) + "\"\r\n";
        socket->write( matchReq.toUtf8() );
        socket->waitForBytesWritten( 1000 );

        if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
          break;

        QString reply;

        if ( !readLine( *socket, reply, errorString, isCancelled ) )
          break;

        if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
          break;

        if ( reply.left( 3 ) == "250" ) {
          // "OK" reply - matches info will be later
          if ( !readLine( *socket, reply, errorString, isCancelled ) )
            break;

          if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
            break;
        }

        if ( reply.left( 3 ) == "552" ) {
          // No matches
          continue;
        }

        if ( reply[ 0 ] == '5' || reply[ 0 ] == '4' ) {
          // Database error
          gdWarning( "Find matches in \"%s\", database \"%s\", strategy \"%s\" fault: %s\n",
                     dict.getName().c_str(),
                     dict.databases.at( i ).toUtf8().data(),
                     dict.strategies.at( ns ).toUtf8().data(),
                     reply.toUtf8().data() );
          continue;
        }

        if ( reply.left( 3 ) == "152" ) {
          // Matches found
          int countPos = reply.indexOf( ' ', 4 );

          // Get matches count
          int count = reply.mid( 4, countPos > 4 ? countPos - 4 : -1 ).toInt();

          // Read matches
          for ( int x = 0; x <= count; x++ ) {
            if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
              break;

            if ( !readLine( *socket, reply, errorString, isCancelled ) )
              break;

            if ( reply[ 0 ] == '.' )
              break;

            while ( reply.endsWith( '\r' ) || reply.endsWith( '\n' ) )
              reply.chop( 1 );

            int pos = reply.indexOf( ' ' );
            if ( pos >= 0 ) {
              QString word = reply.mid( pos + 1 );
              if ( word.endsWith( '\"' ) )
                word.chop( 1 );
              if ( word[ 0 ] == '\"' )
                word = word.mid( 1 );

              matchesList.append( word );
            }
          }
          if ( Utils::AtomicInt::loadAcquire( isCancelled ) || !errorString.isEmpty() )
            break;
        }
      }

      if ( Utils::AtomicInt::loadAcquire( isCancelled ) || !errorString.isEmpty() )
        break;

      matchesList.removeDuplicates();
      if ( matchesList.size() >= MAX_MATCHES_COUNT )
        break;
    }

    if ( !Utils::AtomicInt::loadAcquire( isCancelled ) && errorString.isEmpty() ) {
      matchesList.removeDuplicates();

      int count = matchesList.size();
      if ( count > MAX_MATCHES_COUNT )
        count = MAX_MATCHES_COUNT;

      if ( count ) {
        QMutexLocker _( &dataMutex );
        for ( int x = 0; x < count; x++ )
          matches.emplace_back( gd::toWString( matchesList.at( x ) ) );
      }
    }
  }

  if ( !errorString.isEmpty() )
    gdWarning( "Prefix find in \"%s\" fault: %s\n", dict.getName().c_str(), errorString.toUtf8().data() );

  if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
    socket->abort();
  else
    disconnectFromServer( *socket );

  delete socket;
  socket = nullptr;
  if ( !Utils::AtomicInt::loadAcquire( isCancelled ) )
    finish();
}

void DictServerWordSearchRequest::cancel()
{
  isCancelled.ref();
  finish();
}

class DictServerArticleRequest: public Dictionary::DataRequest
{
  QAtomicInt isCancelled;
  wstring word;
  QString errorString;
  QFuture< void > f;
  DictServerDictionary & dict;
  QTcpSocket * socket;

public:

  DictServerArticleRequest( wstring const & word_, DictServerDictionary & dict_ ):
    word( word_ ),
    dict( dict_ ),
    socket( 0 )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  ~DictServerArticleRequest()
  {
    f.waitForFinished();
  }

  void cancel() override;
};

void DictServerArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  socket = new QTcpSocket;

  if ( !socket ) {
    finish();
    return;
  }

  if ( connectToServer( *socket, dict.url, errorString, isCancelled ) ) {
    string articleData;

    for ( int i = 0; i < dict.databases.size(); i++ ) {
      QString defineReq =
        QString( "DEFINE " ) + dict.databases.at( i ) + " \"" + QString::fromStdU32String( word ) + "\"\r\n";
      socket->write( defineReq.toUtf8() );
      socket->waitForBytesWritten( 1000 );

      QString reply;

      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        break;

      if ( !readLine( *socket, reply, errorString, isCancelled ) )
        break;

      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        break;

      if ( reply.left( 3 ) == "250" ) {
        // "OK" reply - matches info will be later
        if ( !readLine( *socket, reply, errorString, isCancelled ) )
          break;

        if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
          break;
      }

      if ( reply.left( 3 ) == "552" ) {
        // No matches found
        continue;
      }

      if ( reply[ 0 ] == '5' || reply[ 0 ] == '4' ) {
        // Database error
        gdWarning( "Articles request from \"%s\", database \"%s\" fault: %s\n",
                   dict.getName().c_str(),
                   dict.databases.at( i ).toUtf8().data(),
                   reply.toUtf8().data() );
        continue;
      }

      if ( reply.left( 3 ) == "150" ) {
        // Articles found
        int countPos = reply.indexOf( ' ', 4 );

        QString articleText;

        // Get articles count
        int count = reply.mid( 4, countPos > 4 ? countPos - 4 : -1 ).toInt();

        // Read articles
        for ( int x = 0; x < count; x++ ) {
          if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
            break;

          if ( !readLine( *socket, reply, errorString, isCancelled ) )
            break;

          if ( reply.left( 3 ) == "250" )
            break;

          if ( reply.left( 3 ) == "151" ) {
            int pos = 4;
            int endPos;

            // Skip requested word
            if ( reply[ pos ] == '\"' )
              endPos = reply.indexOf( '\"', pos + 1 ) + 1;
            else
              endPos = reply.indexOf( ' ', pos );

            if ( endPos < pos ) {
              // It seems mailformed string
              break;
            }

            pos = endPos + 1;

            QString dbID, dbName;

            // Retrieve database ID
            endPos = reply.indexOf( ' ', pos );

            if ( endPos < pos ) {
              // It seems mailformed string
              break;
            }

            dbID = reply.mid( pos, endPos - pos );

            // Retrieve database ID
            pos    = endPos + 1;
            endPos = reply.indexOf( ' ', pos );
            if ( reply[ pos ] == '\"' )
              endPos = reply.indexOf( '\"', pos + 1 ) + 1;
            else
              endPos = reply.indexOf( ' ', pos );

            if ( endPos < pos ) {
              // It seems mailformed string
              break;
            }

            dbName = reply.mid( pos, endPos - pos );
            if ( dbName.endsWith( '\"' ) )
              dbName.chop( 1 );
            if ( dbName[ 0 ] == '\"' )
              dbName = dbName.mid( 1 );

            articleData += string( "<div class=\"dictserver_from\">From " ) + dbName.toUtf8().data() + " ["
              + dbID.toUtf8().data() + "]:" + "</div>";

            // Retreive MIME headers if any

            static QRegularExpression contentTypeExpr( "Content-Type\\s*:\\s*text/html",
                                                       QRegularExpression::CaseInsensitiveOption );

            bool contentInHtml = false;
            for ( ;; ) {
              if ( !readLine( *socket, reply, errorString, isCancelled ) )
                break;

              if ( reply == "\r\n" )
                break;

              QRegularExpressionMatch match = contentTypeExpr.match( reply );
              if ( match.hasMatch() )
                contentInHtml = true;
            }

            // Retrieve article text

            articleText.clear();
            for ( ;; ) {
              if ( !readLine( *socket, reply, errorString, isCancelled ) )
                break;

              if ( reply == ".\r\n" )
                break;

              articleText += reply;
            }

            if ( Utils::AtomicInt::loadAcquire( isCancelled ) || !errorString.isEmpty() )
              break;

            static QRegularExpression phonetic( R"(\\([^\\]+)\\)",
                                                QRegularExpression::CaseInsensitiveOption ); // phonetics: \stuff\ ...
            static QRegularExpression divs_inside_phonetic( "</div([^>]*)><div([^>]*)>",
                                                            QRegularExpression::CaseInsensitiveOption );
            static QRegularExpression refs( R"(\{([^\{\}]+)\})",
                                            QRegularExpression::CaseInsensitiveOption ); // links: {stuff}
            static QRegularExpression links( "<a href=\"gdlookup://localhost/([^\"]*)\">",
                                             QRegularExpression::CaseInsensitiveOption );
            static QRegularExpression tags( "<[^>]*>", QRegularExpression::CaseInsensitiveOption );

            string articleStr;
            if ( contentInHtml )
              articleStr = articleText.toUtf8().data();
            else
              articleStr = Html::preformat( articleText.toUtf8().data() );

            articleText = QString::fromUtf8( articleStr.c_str(), articleStr.size() );
            if ( !contentInHtml ) {
              articleText = articleText.replace( refs, R"(<a href="gdlookup://localhost/\1">\1</a>)" );

              pos = 0;
              QString articleNewText;

              // Handle phonetics

              QRegularExpressionMatchIterator it = phonetic.globalMatch( articleText );
              while ( it.hasNext() ) {
                QRegularExpressionMatch match = it.next();
                articleNewText += articleText.mid( pos, match.capturedStart() - pos );
                pos = match.capturedEnd();

                QString phonetic_text = match.captured( 1 );
                phonetic_text.replace( divs_inside_phonetic, R"(</span></div\1><div\2><span class="dictd_phonetic">)" );

                articleNewText += "<span class=\"dictd_phonetic\">" + phonetic_text + "</span>";
              }
              if ( pos ) {
                articleNewText += articleText.mid( pos );
                articleText = articleNewText;
                articleNewText.clear();
              }

              // Handle links

              pos = 0;
              it  = links.globalMatch( articleText );
              while ( it.hasNext() ) {
                QRegularExpressionMatch match = it.next();
                articleNewText += articleText.mid( pos, match.capturedStart() - pos );
                pos = match.capturedEnd();

                QString link = match.captured( 1 );
                link.replace( tags, " " );
                link.replace( "&nbsp;", " " );

                QString newLink = match.captured();
                newLink.replace( 30,
                                 match.capturedLength( 1 ),
                                 QString::fromUtf8( QUrl::toPercentEncoding( link.simplified() ) ) );
                articleNewText += newLink;
              }
              if ( pos ) {
                articleNewText += articleText.mid( pos );
                articleText = articleNewText;
                articleNewText.clear();
              }
            }

            articleData += string( "<div class=\"dictd_article\">" ) + articleText.toUtf8().data() + "<br></div>";
          }

          if ( Utils::AtomicInt::loadAcquire( isCancelled ) || !errorString.isEmpty() )
            break;
        }
      }
    }
    if ( !Utils::AtomicInt::loadAcquire( isCancelled ) && errorString.isEmpty() && !articleData.empty() ) {
      appendString( articleData );

      hasAnyData = true;
    }
  }

  if ( !errorString.isEmpty() )
    gdWarning( "Articles request from \"%s\" fault: %s\n", dict.getName().c_str(), errorString.toUtf8().data() );

  if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
    socket->abort();
  else
    disconnectFromServer( *socket );

  delete socket;
  socket = nullptr;
  if ( !Utils::AtomicInt::loadAcquire( isCancelled ) )
    finish();
}

void DictServerArticleRequest::cancel()
{
  isCancelled.ref();

  finish();
}

sptr< WordSearchRequest > DictServerDictionary::prefixMatch( wstring const & word, unsigned long maxResults )
{
  (void)maxResults;
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< WordSearchRequestInstant >();
  }
  else
    return std::make_shared< DictServerWordSearchRequest >( word, *this );
}

sptr< DataRequest >
DictServerDictionary::getArticle( wstring const & word, vector< wstring > const &, wstring const &, bool )

{
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< DataRequestInstant >( false );
  }
  else
    return std::make_shared< DictServerArticleRequest >( word, *this );
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( Config::DictServers const & servers )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & server : servers ) {
    if ( server.enabled )
      result.push_back( std::make_shared< DictServerDictionary >( server.id.toStdString(),
                                                                  server.name.toUtf8().data(),
                                                                  server.url,
                                                                  server.databases,
                                                                  server.strategies,
                                                                  server.iconFilename ) );
  }

  return result;
}

} // namespace DictServer
