/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dictserver.hh"
#include <QTimer>
#include <QUrl>
#include <QTcpSocket>
#include <QString>
#include "htmlescape.hh"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrentRun>

namespace DictServer {

using namespace Dictionary;

enum {
  DefaultPort = 2628
};

namespace {


enum class DictServerState {
  START,
  CONNECT,
  DB,
  DB_DATA,
  DB_DATA_FINISHED,
  CLIENT,
  AUTH,
  OPTION,
  MATCH,
  MATCH_DATA,
  FINISHED,
  DEFINE,
  DEFINE_DATA,
};

#define MAX_MATCHES_COUNT 60


void disconnectFromServer( QTcpSocket & socket )
{
  if ( socket.state() == QTcpSocket::ConnectedState ) {
    socket.write( QByteArray( "QUIT\r\n" ) );
  }

  socket.disconnectFromHost();
}


class DictServerImpl: public QObject
{
  Q_OBJECT

  QString url;
  QString errorString;


  QString msgId;
  QString client;

public:
  QTcpSocket socket;
  DictServerState state;
  QMutex mutex;


  DictServerImpl( QObject * parent, QString const & url_, QString client_ ):
    QObject( parent ),
    url( url_ ),
    client( client_ )
  {
  }

  void run( std::function< void() > callback )
  {
    auto pos = url.indexOf( "://" );
    if ( pos < 0 ) {
      url = "dict://" + url;
    }

    QUrl serverUrl( url );
    quint16 port = serverUrl.port( DefaultPort );
    QString reply;
    socket.connectToHost( serverUrl.host(), port );
    state = DictServerState::CONNECT;
    connect( &socket, &QTcpSocket::connected, this, [ this ]() {} );

    connect( &socket, &QTcpSocket::errorOccurred, this, []( QAbstractSocket::SocketError error ) {
      qDebug() << "socket error message: " << error;
    } );
    connect( &socket, &QTcpSocket::readyRead, this, [ this, callback ]() {
      QMutexLocker const _( &mutex );

      if ( state == DictServerState::CONNECT ) {
        QByteArray reply = socket.readLine();
        qDebug() << "received:" << reply;

        if ( !reply.isEmpty() && reply.left( 3 ) != "220" ) {
          errorString = "Server refuse connection: " + reply;
          return;
        }

        msgId = reply.mid( reply.lastIndexOf( " " ) ).trimmed();

        state = DictServerState::CLIENT;
        socket.write( QString( "CLIENT %1\r\n" ).arg( client ).toUtf8() );
      }

      else if ( state == DictServerState::CLIENT ) {
        QByteArray reply = socket.readLine();
        qDebug() << "received:" << reply;

        QUrl serverUrl( url );
        if ( !serverUrl.userInfo().isEmpty() ) {
          QString authCommand = QString( "AUTH " );
          QString authString  = msgId;

          auto pos = serverUrl.userInfo().indexOf( QRegularExpression( "[:;]" ) );
          if ( pos > 0 ) {
            authCommand += serverUrl.userInfo().left( pos );
            authString += serverUrl.userInfo().mid( pos + 1 );
          }
          else {
            authCommand += serverUrl.userInfo();
          }

          authCommand += " ";
          authCommand += QCryptographicHash::hash( authString.toUtf8(), QCryptographicHash::Md5 ).toHex();
          authCommand += "\r\n";

          state = DictServerState::AUTH;
          socket.write( authCommand.toUtf8() );
        }
        else {
          const QByteArray & data = QByteArray( "OPTION MIME\r\n" );
          socket.write( data );
          qDebug() << "write:" << data;
          state = DictServerState::OPTION;
        }
      }
      else if ( ( state == DictServerState::OPTION ) || ( state == DictServerState::AUTH ) ) {
        QByteArray reply = socket.readLine();
        qDebug() << "received option:" << reply;

        if ( reply.left( 3 ) != "250" ) {
          // RFC 2229, 3.10.1.1:
          // OPTION MIME is a REQUIRED server capability,
          // all DICT servers MUST implement this command.
          errorString = "Server doesn't support mime capability: " + reply;
          return;
        }

        state = DictServerState::FINISHED;

        if ( callback ) {
          callback();
        }
      }
    } );
  }

  ~DictServerImpl() override
  {
    disconnectFromServer( socket );
  }
};


class DictServerDictionary: public Dictionary::Class
{
  Q_OBJECT

  QString url, icon;
  quint32 langId;
  QString errorString;
  QStringList databases;
  QStringList strategies;
  QStringList serverDatabases;
  DictServerState state;
  QMutex mutex;
  QTcpSocket socket;
  QString msgId;

public:
  DictServerDictionary( string const & id,
                        string const & name_,
                        QString const & url_,
                        QString const & database_,
                        QString const & strategies_,
                        QString const & icon_ ):
    Dictionary::Class( id, vector< string >() ),
    url( url_ ),
    icon( icon_ ),
    langId( 0 )
  {

    dictionaryName = name_;

    auto pos = url.indexOf( "://" );
    if ( pos < 0 ) {
      url = "dict://" + url;
    }

    databases = database_.split( QRegularExpression( "[ ,;]" ), Qt::SkipEmptyParts );
    if ( databases.isEmpty() ) {
      databases.append( "*" );
    }

    strategies = strategies_.split( QRegularExpression( "[ ,;]" ), Qt::SkipEmptyParts );
    if ( strategies.isEmpty() ) {
      strategies.append( "prefix" );
    }
    QUrl serverUrl( url );
    quint16 port = serverUrl.port( DefaultPort );
    QString reply;
    socket.connectToHost( serverUrl.host(), port );
    connect( &socket, &QTcpSocket::connected, this, [ this ]() {
      //initialize the description.
      QString req = QString( "SHOW DB\r\n" );
      socket.write( req.toUtf8() );
      state = DictServerState::DB;
    } );

    connect( this, &DictServerDictionary::finishDatabase, this, [ this ]() {
      socket.write( QByteArray( "CLIENT GoldenDict\r\n" ) );
    } );

    connect( &socket, &QTcpSocket::errorOccurred, this, []( QAbstractSocket::SocketError error ) {
      qDebug() << "socket error message: " << error;
    } );
    connect( &socket, &QTcpSocket::readyRead, this, [ this ]() {
      QMutexLocker const _( &mutex );
      QByteArray reply = socket.readLine();
      qDebug() << "received:" << reply;
      if ( state == DictServerState::DB ) {

        if ( reply.left( 3 ) == "110" ) {
          state        = DictServerState::DB_DATA;
          auto countPos = reply.indexOf( ' ', 4 );
          // Get databases count
          int count = reply.mid( 4, countPos > 4 ? countPos - 4 : -1 ).toInt();

          // Read databases
          int x = 0;
          for ( ; x < count; x++ ) {
            reply = socket.readLine();

            if ( reply.isEmpty() ) {
              return;
            }
            reply = reply.trimmed();

            qDebug() << "receive db:" << reply;

            if ( reply[ 0 ] == '.' ) {
              state = DictServerState::DB_DATA_FINISHED;
              emit finishDatabase();
              return;
            }

            if ( !reply.isEmpty() ) {
              serverDatabases.append( reply );
            }
          }

          qDebug() << "db count:" << x;
          if ( x == count ) {
            emit finishDatabase();
          }
        }
      }
      else if ( state == DictServerState::DB_DATA ) {
        while ( !reply.isEmpty() ) {

          qDebug() << "receive db:" << reply;
          if ( reply[ 0 ] == '.' ) {
            state = DictServerState::DB_DATA_FINISHED;
            emit finishDatabase();
            return;
          }

          reply = reply.trimmed();

          if ( !reply.isEmpty() ) {
            serverDatabases.append( reply );
          }

          reply = socket.readLine();
        }
      }
    } );
  }

  ~DictServerDictionary() override
  {
    disconnectFromServer( socket );
  }


  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( std::u32string const &, unsigned long maxResults ) override;

  sptr< DataRequest >
  getArticle( std::u32string const &, vector< std::u32string > const & alts, std::u32string const &, bool ) override;

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

private:
signals:
  void finishDatabase();
};

void DictServerDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !icon.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), icon );
    if ( fInfo.isFile() ) {
      loadIconFromFilePath( fInfo.absoluteFilePath() );
    }
  }
  if ( dictionaryIcon.isNull() ) {
    dictionaryIcon = QIcon( ":/icons/network.svg" );
  }
  dictionaryIconLoaded = true;
}

QString const & DictServerDictionary::getDescription()
{
  if ( dictionaryDescription.isEmpty() ) {
    dictionaryDescription = QCoreApplication::translate( "DictServer", "Url: " ) + url + "<br>";
    dictionaryDescription += QCoreApplication::translate( "DictServer", "Databases: " ) + "<br>";
    for ( const auto & serverDatabase : databases ) {
      dictionaryDescription += serverDatabase + "<br>";
    }
    dictionaryDescription +=
      QCoreApplication::translate( "DictServer", "Search strategies: " ) + strategies.join( ", " );
    if ( !serverDatabases.isEmpty() ) {
      dictionaryDescription += "<br><br>";
      dictionaryDescription += QCoreApplication::translate( "DictServer", "Server databases" ) + " ("
        + QString::number( serverDatabases.size() ) + "):" + "<br>";
      for ( const auto & serverDatabase : serverDatabases ) {
        dictionaryDescription += serverDatabase + "<br>";
      }
    }
  }
  return dictionaryDescription;
}


class DictServerWordSearchRequest: public Dictionary::WordSearchRequest
{
  Q_OBJECT
  QAtomicInt isCancelled;
  std::u32string word;
  QString errorString;
  DictServerDictionary & dict;

  QStringList matchesList;
  int currentStrategy = 0;
  int currentDatabase = 0;

  DictServerState state = DictServerState::START;
  QString msgId;

  DictServerImpl * dictImpl;

public:

  DictServerWordSearchRequest( std::u32string word_, DictServerDictionary & dict_ ):
    word( std::move( word_ ) ),
    dict( dict_ ),
    dictImpl( new DictServerImpl( this, dict_.url, "GoldenDict-w" ) )
  {

    connect( this, &DictServerWordSearchRequest::finishedMatches, this, [ this ]() {
      state = DictServerState::FINISHED;

      matchesList.removeDuplicates();
      int countn = qMin( matchesList.size(), MAX_MATCHES_COUNT );

      if ( countn ) {
        QMutexLocker _( &dataMutex );
        for ( int x = 0; x < countn; x++ ) {
          matches.emplace_back( matchesList.at( x ).toStdU32String() );
        }
      }
      finish();
    } );

    this->run();

    dictImpl->run( [ this ]() {
      state = dictImpl->state;

      matchNext();
    } );
  }

  void matchNext()
  {
    if ( currentDatabase >= dict.databases.size() ) {
      currentStrategy++;
      currentDatabase = 0;
    }
    if ( currentStrategy >= dict.strategies.size() ) {
      emit finishedMatches();
      return;
    }
    state = DictServerState::MATCH;

    QString matchReq = QString( "MATCH " ) + dict.databases.at( currentDatabase ) + " "
      + dict.strategies.at( currentStrategy ) + " \"" + QString::fromStdU32String( word ) + "\"\r\n";
    dictImpl->socket.write( matchReq.toUtf8() );
    currentDatabase++;
  }

  void run();

  ~DictServerWordSearchRequest() override = default;

  void cancel() override;

  void addMatchedWord( const QString & );

private:
  void readMatchData( QByteArray & reply );

signals:
  void finishedMatches();
};

void DictServerWordSearchRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  connect( &dictImpl->socket, &QTcpSocket::readyRead, this, [ this ]() {
    QMutexLocker const _( &dictImpl->mutex );
    if ( state == DictServerState::MATCH ) {
      QByteArray reply = dictImpl->socket.readLine();
      qDebug() << "receive match:" << reply;
      auto code = reply.left( 3 );

      if ( code != "152" ) {

        matchNext();
      }
      else {
        state = DictServerState::MATCH_DATA;

        readMatchData( reply );
      }
    }
    else if ( state == DictServerState::MATCH_DATA ) {
      QByteArray reply = dictImpl->socket.readLine();
      qDebug() << "receive match data:" << reply;
      readMatchData( reply );
    }
  } );

  connect( &dictImpl->socket, &QTcpSocket::errorOccurred, this, [ this ]( QAbstractSocket::SocketError error ) {
    qDebug() << "socket error message: " << error;
    cancel();
  } );
}
void DictServerWordSearchRequest::readMatchData( QByteArray & reply )
{
  do {

    if ( reply.isEmpty() ) {
      return;
    }

    reply = reply.trimmed();

    if ( reply == "." ) {
      //discard left response.  such as
      // 250 ok [d/m/c = 0/3/20; 0.000r 0.000u 0.000s]
      while ( !dictImpl->socket.readLine().isEmpty() ) {}
      matchNext();
      return;
    }

    if ( reply.left( 3 ) == "152" ) {
      reply = this->dictImpl->socket.readLine();

      continue;
    }

    auto pos = reply.indexOf( ' ' );
    if ( pos >= 0 ) {
      QString word = reply.mid( pos + 1 );
      if ( word.endsWith( '\"' ) ) {
        word.chop( 1 );
      }
      if ( word.startsWith( '\"' ) ) {
        word = word.remove( 0, 1 );
      }

      if ( !word.isEmpty() ) {
        this->addMatchedWord( word );
      }
    }

    reply = this->dictImpl->socket.readLine();

  } while ( true );
}

void DictServerWordSearchRequest::cancel()
{
  isCancelled.ref();
  finish();
}
void DictServer::DictServerWordSearchRequest::addMatchedWord( const QString & str )
{
  matchesList.append( str );

  if ( matchesList.size() >= MAX_MATCHES_COUNT ) {
    emit finishedMatches();
  }
}

class DictServerArticleRequest: public Dictionary::DataRequest
{
  QAtomicInt isCancelled;
  std::u32string word;
  QString errorString;
  DictServerDictionary & dict;
  string articleData;

  QString articleText;

  int currentDatabase = 0;
  DictServerState state;
  QTimer * timer;
  bool contentInHtml = false;


public:

  DictServerImpl * dictImpl;
  DictServerArticleRequest( std::u32string word_, DictServerDictionary & dict_ ):
    word( std::move( word_ ) ),
    dict( dict_ ),
    dictImpl( new DictServerImpl( this, dict_.url, "GoldenDict-t" ) )
  {
    timer = new QTimer( this );
    timer->setInterval( 5000 );
    timer->setSingleShot( true );
    qDebug() << "receive data:" << QDateTime::currentDateTime();
    connect( timer, &QTimer::timeout, this, [ this ]() {
      qDebug() << "Server takes too much time to response" << QDateTime::currentDateTime();
      cancel();
    } );

    connect( this, &DictServerArticleRequest::finishedArticle, this, [ this ]( QString _articleText ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        cancel();
        return;
      }

      //modify the _articleText,remove extra lines[start with 15X etc.]
      QList< QString > lines = _articleText.split( "\n", Qt::SkipEmptyParts );

      QString resultStr;

      // process the line
      static QRegularExpression leadingRespCode( "^\\d{3} " );
      uint32_t leadingSpaceCount      = 0;
      uint32_t firstLeadingSpaceCount = 0;
      for ( const QString & line : lines ) {
        //ignore 15X lines
        if ( leadingRespCode.match( line ).hasMatch() ) {
          continue;
        }
        // ignore dot(.),the end line character
        if ( line.trimmed() == "." ) {
          break;
        }

        auto lsc = Utils::leadingSpaceCount( line );

        if ( firstLeadingSpaceCount == 0 && lsc > firstLeadingSpaceCount ) {
          firstLeadingSpaceCount = lsc;
        }

        if ( lsc >= leadingSpaceCount && lsc > firstLeadingSpaceCount ) {
          //extra space
          resultStr.append( " " );
          resultStr.append( line.trimmed() );
        }
        else {
          resultStr.append( "\n" );
          resultStr.append( line );
        }
        leadingSpaceCount = lsc;
      }

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
      if ( contentInHtml ) {
        articleStr = resultStr.toUtf8().data();
      }
      else {
        articleStr = Html::preformat( resultStr.toUtf8().data() );
      }

      _articleText = QString::fromUtf8( articleStr.c_str(), articleStr.size() );
      qsizetype pos;
      if ( !contentInHtml ) {
        _articleText = _articleText.replace( refs, R"(<a href="gdlookup://localhost/\1">\1</a>)" );

        pos = 0;
        QString articleNewText;

        // Handle phonetics

        QRegularExpressionMatchIterator it = phonetic.globalMatch( _articleText );
        while ( it.hasNext() ) {
          QRegularExpressionMatch match = it.next();
          articleNewText += _articleText.mid( pos, match.capturedStart() - pos );
          pos = match.capturedEnd();

          QString phonetic_text = match.captured( 1 );
          phonetic_text.replace( divs_inside_phonetic, R"(</span></div\1><div\2><span class="dictd_phonetic">)" );

          articleNewText += R"(<span class="dictd_phonetic">)" + phonetic_text + "</span>";
        }
        if ( pos ) {
          articleNewText += _articleText.mid( pos );
          _articleText = articleNewText;
          articleNewText.clear();
        }

        // Handle links

        pos = 0;
        it  = links.globalMatch( _articleText );
        while ( it.hasNext() ) {
          QRegularExpressionMatch match = it.next();
          articleNewText += _articleText.mid( pos, match.capturedStart() - pos );
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
          articleNewText += _articleText.mid( pos );
          _articleText = articleNewText;
          articleNewText.clear();
        }
      }

      articleData += string( "<div class=\"dictd_article\">" ) + _articleText.toUtf8().data() + "<br></div>";


      if ( !articleData.empty() ) {
        appendString( articleData );
        articleData.clear();

        hasAnyData = true;
      }

      defineNext();
    } );
    this->run();

    dictImpl->run( [ this ]() {
      state = dictImpl->state;
      state = DictServerState::DEFINE;

      defineNext();
    } );

    timer->start();
  }

  void run();

  ~DictServerArticleRequest() override = default;

  void cancel() override;
  void readData( QByteArray reply );
  bool defineNext();
};

bool DictServerArticleRequest::defineNext()
{
  if ( currentDatabase >= dict.databases.size() ) {
    finish();
    return false;
  }
  QString defineReq =
    QString( "DEFINE " ) + dict.databases.at( currentDatabase ) + " \"" + QString::fromStdU32String( word ) + "\"\r\n";
  dictImpl->socket.write( defineReq.toUtf8() );
  qDebug() << "define req:" << defineReq;
  currentDatabase++;
  return true;
}

void DictServerArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  connect( &dictImpl->socket, &QTcpSocket::readyRead, this, [ this ]() {
    QMutexLocker const _( &dictImpl->mutex );
    timer->start();
    if ( state == DictServerState::DEFINE ) {
      QByteArray reply = dictImpl->socket.readLine();
      qDebug() << "receive define:" << reply;

      //work around to fix ,some extra response .
      if ( reply.left( 3 ) == "250" ) {
        reply = dictImpl->socket.readLine();
        if ( reply.isEmpty() ) {
          return;
        }
      }

      //no match
      if ( reply.left( 3 ) == "552" ) {
        defineNext();
        return;
      }

      auto code = reply.left( 3 );
      if ( reply.left( 3 ) == "150" ) {
        // Articles found
        auto countPos = reply.indexOf( ' ', 4 );
        // Get articles count,
        // todo ,how to use this count?
        int count = reply.mid( 4, countPos > 4 ? countPos - 4 : -1 ).toInt();

        // Read articles
        readData( reply );
        state = DictServerState::DEFINE_DATA;
      }
    }
    else if ( state == DictServerState::DEFINE_DATA ) {
      QByteArray reply = dictImpl->socket.readLine();
      qDebug() << "receive define data:" << reply << QDateTime::currentDateTime();
      readData( reply );
    }
  } );

  connect( &dictImpl->socket, &QTcpSocket::errorOccurred, this, [ this ]( QAbstractSocket::SocketError error ) {
    qDebug() << "socket error message: " << error;
    cancel();
  } );
}

void DictServerArticleRequest::readData( QByteArray reply )
{
  if ( reply.isEmpty() ) {
    return;
  }

  if ( reply.left( 3 ) == "250" ) {
    return;
  }

  if ( reply.left( 3 ) == "151" ) {
    qsizetype pos = 4;
    qsizetype endPos;

    // Skip requested word
    if ( reply[ pos ] == '\"' ) {
      endPos = reply.indexOf( '\"', pos + 1 ) + 1;
    }
    else {
      endPos = reply.indexOf( ' ', pos );
    }

    if ( endPos < pos ) {
      // It seems mailformed string
      return;
    }

    pos = endPos + 1;

    QString dbID;
    QString dbName;

    // Retrieve database ID
    endPos = reply.indexOf( ' ', pos );

    if ( endPos < pos ) {
      // It seems mailformed string
      return;
    }

    dbID = reply.mid( pos, endPos - pos );

    // Retrieve database ID
    pos = endPos + 1;
    if ( reply[ pos ] == '\"' ) {
      endPos = reply.indexOf( '\"', pos + 1 ) + 1;
    }
    else {
      endPos = reply.indexOf( ' ', pos );
    }

    if ( endPos < pos ) {
      // It seems mailformed string
      return;
    }

    dbName = reply.mid( pos, endPos - pos );
    if ( dbName.endsWith( '\"' ) ) {
      dbName.chop( 1 );
    }
    if ( dbName[ 0 ] == '\"' ) {
      dbName = dbName.mid( 1 );
    }

    articleData += string( "<div class=\"dictserver_from\">" ) + dbName.toUtf8().data() + "[" + dbID.toUtf8().data()
      + "]" + "</div>";

    reply = dictImpl->socket.readAll();

    articleText += reply;
    qDebug() << "reply data:" << reply << QDateTime::currentDateTime();
    if ( articleText.contains( "\r\n.\r\n" ) ) {
      //discard all left message.
      emit finishedArticle( articleText );
      return;
    }
  }
  else {
    articleText += reply;
    reply = dictImpl->socket.readAll();
    qDebug() << "reply data:" << reply << QDateTime::currentDateTime();

    articleText += reply;
    if ( reply.contains( "\r\n.\r\n" ) ) {
      //discard all left message. maybe delete all the remaining data after `.\r\n`
      emit finishedArticle( articleText );
      return;
    }
  }
  //restart.
  timer->start();
}

void DictServerArticleRequest::cancel()
{
  isCancelled.ref();

  finish();
}

sptr< WordSearchRequest > DictServerDictionary::prefixMatch( std::u32string const & word, unsigned long maxResults )
{
  (void)maxResults;
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< WordSearchRequestInstant >();
  }
  else {
    return std::make_shared< DictServerWordSearchRequest >( word, *this );
  }
}

sptr< DataRequest > DictServerDictionary::getArticle( std::u32string const & word,
                                                      vector< std::u32string > const &,
                                                      std::u32string const &,
                                                      bool )

{
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< DataRequestInstant >( false );
  }
  else {
    return std::make_shared< DictServerArticleRequest >( word, *this );
  }
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( Config::DictServers const & servers )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & server : servers ) {
    if ( server.enabled ) {
      result.push_back( std::make_shared< DictServerDictionary >( server.id.toStdString(),
                                                                  server.name.toUtf8().data(),
                                                                  server.url,
                                                                  server.databases,
                                                                  server.strategies,
                                                                  server.iconFilename ) );
    }
  }

  return result;
}
#include "dictserver.moc"
} // namespace DictServer
