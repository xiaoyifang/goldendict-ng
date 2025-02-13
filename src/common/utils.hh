/* Thin wrappers for retaining compatibility for both Qt6.x and Qt5.x */

#pragma once

#include <QAtomicInt>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QString>
#include <QTextDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QWidget>
#include "filetype.hh"
#include <string>
using std::string;

namespace Utils {
QMap< QString, QString > str2map( const QString & contextsEncoded );

inline bool isCJKChar( ushort ch )
{
  if ( ( ch >= 0x3400 && ch <= 0x9FFF ) || ( ch >= 0xF900 && ch <= 0xFAFF ) || ( ch >= 0xD800 && ch <= 0xDFFF ) )
    return true;

  return false;
}
/**
 * remove right end space
 */
inline QString rstrip( const QString & str )
{
  int n = str.size() - 1;
  for ( ; n >= 0; --n ) {
    if ( !str.at( n ).isSpace() ) {
      return str.left( n + 1 );
    }
  }
  return {};
}

inline uint32_t leadingSpaceCount( const QString & str )
{
  for ( int i = 0; i < str.size(); i++ ) {
    if ( str.at( i ).isSpace() ) {
      continue;
    }
    else {
      return i;
    }
  }
  return 0;
}

std::string c_string( const QString & str );
bool endsWithIgnoreCase( QByteArrayView str, QByteArrayView extension );
/**
 * remove punctuation , space, symbol
 *
 *
 * " abc, '" should be "abc"
 */
inline QString trimNonChar( const QString & str )
{
  QString remain;
  int n = str.size() - 1;
  for ( ; n >= 0; --n ) {
    auto c = str.at( n );
    if ( !c.isSpace() && !c.isSymbol() && !c.isNonCharacter() && !c.isPunct() && !c.isNull() ) {
      remain = str.left( n + 1 );
      break;
    }
  }

  n = 0;
  for ( ; n < remain.size(); n++ ) {
    auto c = remain.at( n );
    if ( !c.isSpace() && !c.isSymbol() && !c.isNonCharacter() && !c.isPunct() ) {
      return remain.mid( n );
    }
  }

  return "";
}

/**
 * str="abc\r\n\u0000" should be returned as "abc"
 * @brief rstripnull
 * @param str
 * @return
 */
inline QString rstripnull( const QString & str )
{
  int n = str.size() - 1;
  for ( ; n >= 0; --n ) {
    auto c = str.at( n );
    if ( !c.isSpace() && !c.isNull() ) {
      return str.left( n + 1 );
    }
  }
  return "";
}

inline bool isExternalLink( QUrl const & url )
{
  return url.scheme() == "http" || url.scheme() == "https" || url.scheme() == "ftp" || url.scheme() == "mailto"
    || url.scheme() == "file" || url.toString().startsWith( "//" );
}

inline bool isHtmlResources( QUrl const & url )
{
  auto fileName   = url.fileName();
  qsizetype index = fileName.lastIndexOf( "." );
  QStringList extensions{ ".css", ".woff", ".woff2", ".ttf", ".otf", ".bmp", ".jpg",  ".png", ".gif", ".tif",
                          ".wav", ".ogg",  ".oga",   ".mp3", ".mp4", ".aac", ".flac", ".mid", ".wv",  ".ape" };

  if ( index > -1 ) {
    auto ext = fileName.mid( index );
    if ( extensions.contains( ext, Qt::CaseInsensitive ) )
      return true;
  }
  // some url has the form like  https://xxxx/audio?file=***.mp3&a=1 etc links.
  for ( QString extension : extensions ) {
    if ( url.url().contains( extension, Qt::CaseInsensitive ) )
      return true;
  }
  return false;
}

inline QString escape( QString const & plain )
{
  return plain.toHtmlEscaped();
}

// should ignore key event.
inline bool ignoreKeyEvent( QKeyEvent * keyEvent )
{
  if ( keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Tab
       || keyEvent->key() == Qt::Key_Backtab || keyEvent->key() == Qt::Key_Escape )
    return true;
  return false;
}

inline QString json2String( const QJsonObject & json )
{
  return QString( QJsonDocument( json ).toJson( QJsonDocument::Compact ) );
}

inline QStringList repeat( const QString str, const int times )
{
  QStringList list;
  for ( int i = 0; i < times; i++ ) {
    list << str;
  }
  return list;
}

namespace AtomicInt {

inline int loadAcquire( QAtomicInt const & ref )
{
  return ref.loadAcquire();
}

} // namespace AtomicInt

namespace Url {

// This wrapper is created due to behavior change of the setPath() method
// See: https://bugreports.qt-project.org/browse/QTBUG-27728
//       https://codereview.qt-project.org/#change,38257
inline QString ensureLeadingSlash( const QString & path )
{
  QLatin1Char slash( '/' );
  if ( path.startsWith( slash ) )
    return path;
  return slash + path;
}

inline bool hasQueryItem( QUrl const & url, QString const & key )
{
  return QUrlQuery( url ).hasQueryItem( key );
}

inline QString queryItemValue( QUrl const & url, QString const & item )
{
  return QUrlQuery( url ).queryItemValue( item, QUrl::FullyDecoded );
}

inline QByteArray encodedQueryItemValue( QUrl const & url, QString const & item )
{
  return QUrlQuery( url ).queryItemValue( item, QUrl::FullyEncoded ).toLatin1();
}

inline void addQueryItem( QUrl & url, QString const & key, QString const & value )
{
  QUrlQuery urlQuery( url );
  urlQuery.addQueryItem( key, value );
  url.setQuery( urlQuery );
}

inline void removeQueryItem( QUrl & url, QString const & key )
{
  QUrlQuery urlQuery( url );
  urlQuery.removeQueryItem( key );
  url.setQuery( urlQuery );
}

inline void setQueryItems( QUrl & url, QList< std::pair< QString, QString > > const & query )
{
  QUrlQuery urlQuery( url );
  urlQuery.setQueryItems( query );
  url.setQuery( urlQuery );
}

inline QString path( QUrl const & url )
{
  return url.path( QUrl::FullyDecoded );
}

inline void setFragment( QUrl & url, const QString & fragment )
{
  url.setFragment( fragment, QUrl::DecodedMode );
}

inline QString fragment( const QUrl & url )
{
  return url.fragment( QUrl::FullyDecoded );
}

// get the query word of bword and gdlookup scheme.
// if the scheme is gdlookup or scheme ,the first value of pair is true,otherwise is false;
inline std::pair< bool, QString > getQueryWord( QUrl const & url )
{
  QString word;
  bool validScheme = false;
  if ( url.scheme().compare( "gdlookup" ) == 0 ) {
    validScheme = true;
    if ( hasQueryItem( url, "word" ) ) {
      word = queryItemValue( url, "word" );
    }
    else {
      word = url.path().mid( 1 );
    }
  }
  if ( url.scheme().compare( "bword" ) == 0 || url.scheme().compare( "entry" ) == 0 ) {
    validScheme = true;

    auto path = url.path();
    // url like this , bword:word  or bword://localhost/word
    if ( !path.isEmpty() ) {
      //url,bword://localhost/word
      if ( path.startsWith( "/" ) )
        word = path.mid( 1 );
      else
        word = path;
    }
    else {
      // url looks like this, bword://word,or bword://localhost
      auto host = url.host();
      if ( host != "localhost" ) {
        word = host;
      }
    }
  }
  return std::make_pair( validScheme, word );
}

inline bool isAudioUrl( QUrl const & url )
{
  if ( !url.isValid() )
    return false;

  // gdau links are known to be audios, (sometimes they may not have file extension).
  if ( url.scheme() == "gdau" || url.scheme() == "gdprg" || url.scheme() == "gdtts" ) {
    return true;
  }

  // Note: we check for forvo sound links explicitly, as they don't have extensions
  return ( url.scheme() == "http" || url.scheme() == "https" )
    && ( Filetype::isNameOfSound( url.path().toUtf8().data() ) || url.host() == "apifree.forvo.com" );
}

inline bool isWebAudioUrl( QUrl const & url )
{
  if ( !url.isValid() )
    return false;
  // Note: we check for forvo sound links explicitly, as they don't have extensions

  return ( url.scheme() == "http" || url.scheme() == "https" )
    && ( Filetype::isNameOfSound( url.path().toUtf8().data() ) || url.host() == "apifree.forvo.com" );
}

/// Uses some heuristics to chop off the first domain name from the host name,
/// but only if it's not too base. Returns the resulting host name.
inline QString getHostBase( QString const & host )
{
  QStringList domains = host.split( '.' );

  int left = domains.size();

  // Skip last <=3-letter domain name
  if ( left && domains[ left - 1 ].size() <= 3 )
    --left;

  // Skip another <=3-letter domain name
  if ( left && domains[ left - 1 ].size() <= 3 )
    --left;

  if ( left > 1 ) {
    // We've got something like www.foobar.co.uk -- we can chop off the first
    // domain

    return host.mid( domains[ 0 ].size() + 1 );
  }
  else
    return host;
}

inline QString getHostBaseFromUrl( QUrl const & url )
{
  QString host = url.host();

  return getHostBase( host );
}

QString getSchemeAndHost( QUrl const & url );

} // namespace Url

namespace Path {
QString combine( const QString & path1, const QString & path2 );
}

namespace Widget {
void setNoResultColor( QWidget * widget, bool noResult );
}

namespace Html {
// See Issue #271: A mechanism to clean-up invalid HTML cards.
std::string getHtmlCleaner();
} // namespace Html

/// Utilities to convert a wide string or an utf8 string to the local 8bit
/// encoding of the file system, and to do other manipulations on the file
/// names.
namespace Fs {

using std::string;

/// Returns the filesystem separator (/ on Unix and clones, \ on Windows).
char separator();

/// Returns the name part of the given filename.
string basename( string const & );
void removeDirectory( QString const & directory );

void removeDirectory( string const & directory );

inline QString findFirstExistingFile( std::initializer_list< QString > filePaths )
{
  for ( const QString & filePath : filePaths ) {
    if ( QFileInfo::exists( filePath ) ) {
      return filePath;
    }
  }
  return QString();
}

inline std::string findFirstExistingFile( std::initializer_list< std::string > filePaths )
{
  for ( const std::string & filePath : filePaths ) {
    auto fp = QString::fromStdString( filePath );
    if ( QFileInfo::exists( fp ) ) {
      return filePath;
    }
  }
  return {};
}

inline bool anyExistingFile( std::initializer_list< std::string > filePaths )
{
  for ( const std::string & filePath : filePaths ) {
    auto fp = QString::fromStdString( filePath );
    if ( QFileInfo::exists( fp ) ) {
      return true;
    }
  }
  return false;
}

// used for std::string and char*
inline bool exists( std::string_view filename ) noexcept
{
  return QFileInfo::exists( QString::fromUtf8( filename.data(), filename.size() ) );
}

} // namespace Fs

namespace WebSite {
QString urlReplaceWord( const QString url, QString word );
}

QString escapeAmps( QString const & str );

QString unescapeAmps( QString const & str );

} // namespace Utils
