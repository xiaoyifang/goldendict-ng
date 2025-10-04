/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <vector>
#include <algorithm>
#include <cstdio>
#include "dictionary.hh"

// For needToRebuildIndex(), read below
#include <QFileInfo>
#include <QDateTime>

#include "config.hh"
#include <QDir>
#include <QCryptographicHash>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QRegularExpression>
#include "utils.hh"
#include "zipfile.hh"
#include <array>

namespace Dictionary {

bool Request::isFinished()
{
  return Utils::AtomicInt::loadAcquire( isFinishedFlag );
}

void Request::update()
{
  if ( !Utils::AtomicInt::loadAcquire( isFinishedFlag ) ) {
    emit updated();
  }
}

void Request::finish()
{
  if ( !Utils::AtomicInt::loadAcquire( isFinishedFlag ) ) {
    {
      QMutexLocker _( &dataMutex );
      isFinishedFlag.ref();

      cond.wakeAll();
    }
    emit finished();
  }
}

void Request::setErrorString( const QString & str )
{
  QMutexLocker _( &errorStringMutex );

  errorString = str;
}

QString Request::getErrorString()
{
  QMutexLocker _( &errorStringMutex );

  return errorString;
}


///////// WordSearchRequest

size_t WordSearchRequest::matchesCount()
{
  QMutexLocker _( &dataMutex );

  return matches.size();
}

WordMatch WordSearchRequest::operator[]( size_t index )
{
  QMutexLocker _( &dataMutex );

  if ( index >= matches.size() ) {
    throw exIndexOutOfRange();
  }

  return matches[ index ];
}

vector< WordMatch > & WordSearchRequest::getAllMatches()
{
  if ( !isFinished() ) {
    throw exRequestUnfinished();
  }

  return matches;
}

void WordSearchRequest::addMatch( const WordMatch & match )
{
  QMutexLocker _( &dataMutex );

  // Check if the match already exists
  if ( std::find( matches.begin(), matches.end(), match ) == matches.end() ) {
    matches.push_back( match );
  }
}

////////////// DataRequest

long DataRequest::dataSize()
{
  QMutexLocker _( &dataMutex );
  long size = hasAnyData ? (long)data.size() : -1;

  if ( size == 0 && !isFinished() ) {
    cond.wait( &dataMutex );
    size = hasAnyData ? (long)data.size() : -1;
  }
  return size;
}

void DataRequest::appendDataSlice( const void * buffer, size_t size )
{
  QMutexLocker _( &dataMutex );

  size_t offset = data.size();

  data.resize( data.size() + size );

  memcpy( &data.front() + offset, buffer, size );
  cond.wakeAll();
}

void DataRequest::appendString( std::string_view str )
{
  QMutexLocker _( &dataMutex );
  data.reserve( data.size() + str.size() );
  data.insert( data.end(), str.begin(), str.end() );
  cond.wakeAll();
}

void DataRequest::getDataSlice( size_t offset, size_t size, void * buffer )
{
  if ( size == 0 ) {
    return;
  }
  QMutexLocker _( &dataMutex );

  if ( !hasAnyData ) {
    throw exSliceOutOfRange();
  }

  memcpy( buffer, &data[ offset ], size );
}

vector< char > & DataRequest::getFullData()
{
  if ( !isFinished() ) {
    throw exRequestUnfinished();
  }

  return data;
}

Class::Class( const string & id_, const vector< string > & dictionaryFiles_ ):
  id( id_ ),
  dictionaryFiles( dictionaryFiles_ ),
  indexedFtsDoc( 0 ),
  dictionaryIconLoaded( false ),
  can_FTS( false ),
  FTS_index_completed( false )
{
}

void Class::deferredInit()
{
  //base method.
}

sptr< WordSearchRequest > Class::stemmedMatch( const std::u32string & /*str*/,
                                               unsigned /*minLength*/,
                                               unsigned /*maxSuffixVariation*/,
                                               unsigned long /*maxResults*/ )
{
  return std::make_shared< WordSearchRequestInstant >();
}

sptr< WordSearchRequest > Class::findHeadwordsForSynonym( const std::u32string & )
{
  return std::make_shared< WordSearchRequestInstant >();
}

vector< std::u32string > Class::getAlternateWritings( const std::u32string & ) noexcept
{
  return {};
}

QString Class::getContainingFolder() const
{
  if ( !dictionaryFiles.empty() ) {
    auto fileInfo = QFileInfo( QString::fromStdString( dictionaryFiles[ 0 ] ) );
    if ( fileInfo.isDir() ) {
      return fileInfo.absoluteFilePath();
    }
    return fileInfo.absolutePath();
  }

  return {};
}

sptr< DataRequest > Class::getResource( const string & /*name*/ )

{
  return std::make_shared< DataRequestInstant >( false );
}

sptr< DataRequest > Class::getSearchResults( const QString &, int, bool, bool )
{
  return std::make_shared< DataRequestInstant >( false );
}

const QString & Class::getDescription()
{
  return dictionaryDescription;
}

QString Class::getMainFilename()
{
  return {};
}

const QIcon & Class::getIcon() noexcept
{
  if ( !dictionaryIconLoaded ) {
    loadIcon();
  }
  return dictionaryIcon;
}

void Class::loadIcon() noexcept
{
  dictionaryIconLoaded = true;
}

int Class::getOptimalIconSize()
{
  return 64 * qGuiApp->devicePixelRatio();
}

bool Class::loadIconFromFileName( const QString & mainDictFileName )
{
  const QFileInfo info( mainDictFileName );
  QDir dir = info.absoluteDir();

  dir.setFilter( QDir::Files );
  dir.setNameFilters( QStringList() << "*.bmp"  //
                                    << "*.png"  //
                                    << "*.jpg"  //
                                    << "*.ico"  // below are GD-ng only
                                    << "*.jpeg" //
                                    << "*.gif"  //
                                    << "*.webp" //
                                    << "*.svg"  //
                                    << "*.svgz" );

  const QString basename = info.baseName();
  for ( const auto & f : dir.entryInfoList() ) {
    if ( f.baseName() == basename && loadIconFromFilePath( f.absoluteFilePath() ) ) {
      return true;
    }
  }
  return false;
}

bool Class::loadIconFromFilePath( const QString & filename )
{
  auto iconSize = getOptimalIconSize();
  QImage img( filename );

  if ( img.isNull() ) {
    return false;
  }
  else {
    auto result    = img.scaled( { iconSize, iconSize }, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
    dictionaryIcon = QIcon( QPixmap::fromImage( result ) );

    return !dictionaryIcon.isNull();
  }
}

bool Class::loadIconFromText( const QString & iconUrl, const QString & text )
{
  //select a single char.
  auto abbrName = getAbbrName( text );
  if ( abbrName.isEmpty() ) {
    return false;
  }
  QImage img( iconUrl );

  if ( !img.isNull() ) {
    auto iconSize = getOptimalIconSize();

    QImage result = img.scaled( { iconSize, iconSize }, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );

    QPainter painter( &result );
    painter.setRenderHints( QPainter::Antialiasing | QPainter::TextAntialiasing );
    painter.setCompositionMode( QPainter::CompositionMode_SourceAtop );

    QFont font = painter.font();
    //the orderNum should be a little smaller than the icon
    font.setPixelSize( iconSize * 0.8 );
    font.setWeight( QFont::Bold );
    painter.setFont( font );

    const QRect rectangle = QRect( 0, 0, iconSize, iconSize );

    painter.setPen( intToFixedColor( qHash( abbrName ) ) );

    // Draw first character
    painter.drawText( rectangle, Qt::AlignCenter, abbrName.at( 0 ) );

    //the orderNum should be a little smaller than the icon
    font.setPixelSize( iconSize * 0.4 );
    QFontMetrics fm1( font );
    const QString & orderNum = abbrName.mid( 1 );

    painter.setFont( font );
    painter.drawText( rectangle, Qt::AlignRight | Qt::AlignBottom, orderNum );

    painter.end();

    dictionaryIcon = QIcon( QPixmap::fromImage( result ) );

    return !dictionaryIcon.isNull();
  }
  return false;
}

QColor Class::intToFixedColor( int index )
{
  // Predefined list of colors
  static const std::array colors = {
    QColor( 255, 0, 0, 200 ),     // Red
    QColor( 4, 57, 108, 200 ),    //Custom
    QColor( 0, 255, 0, 200 ),     // Green
    QColor( 0, 0, 255, 200 ),     // Blue
    QColor( 255, 255, 0, 200 ),   // Yellow
    QColor( 0, 255, 255, 200 ),   // Cyan
    QColor( 255, 0, 255, 200 ),   // Magenta
    QColor( 192, 192, 192, 200 ), // Gray
    QColor( 255, 165, 0, 200 ),   // Orange
    QColor( 128, 0, 128, 200 ),   // Violet
    QColor( 128, 128, 0, 200 )    // Olive
  };

  // Use modulo operation to ensure index is within the range of the color list
  return colors[ index % colors.size() ];
}

QString Class::getAbbrName( const QString & text )
{
  return GlobalBroadcaster::instance()->getAbbrName( text );
}

// Forward declaration
int findMatchingBracket( const QString & css, int startPos );

void Class::isolateCSS( QString & css, const QString & wrapperSelector )
{
  // Return early if CSS is empty
  if ( css.isEmpty() ) {
    return;
  }

  if ( css.indexOf( "{" ) == -1 ) {
    return;
  }

  QString newCSS;
  int currentPos = 0;

  // Create isolation prefix using dictionary ID
  QString prefix( "#gdarticlefrom-" );
  prefix += QString::fromLatin1( getId().c_str() );
  if ( !wrapperSelector.isEmpty() ) {
    prefix += " " + wrapperSelector;
  }

  // Regular expressions for CSS parsing
  QRegularExpression commentRegex( R"(\/\*(?:.(?!\*\/))*.?\*\/)", QRegularExpression::DotMatchesEverythingOption );
  QRegularExpression selectorSeparatorRegex( R"([ \*\>\+,;:\[\{\]])" );
  QRegularExpression selectorEndRegex( "[,\\{]" );

  // Remove comments from CSS
  css.replace( commentRegex, QString() );

  // Replace pseudo root selector with html
  css.replace( QRegularExpression( R"(:root\s*{)" ), "html{" );

  // Process CSS content
  while ( currentPos < css.length() ) {
    QChar ch = css.at( currentPos );

    if ( ch == '@' ) {
      // Handle @ rules
      int ruleNameEnd = css.indexOf( QRegularExpression( "[^\\w-]" ), currentPos + 1 );
      if ( ruleNameEnd == -1 ) {
        // If no rule name end is found, copy remaining content
        newCSS.append( css.mid( currentPos ) );
        break;
      }

      QString ruleName = css.mid( currentPos, ruleNameEnd - currentPos ).trimmed();

      // Special handling for rules that don't need modification
      if ( ruleName.compare( "@import", Qt::CaseInsensitive ) == 0
           || ruleName.compare( "@charset", Qt::CaseInsensitive ) == 0 ) {
        // Find semicolon as end marker
        int semicolonPos = css.indexOf( ';', currentPos );
        if ( semicolonPos != -1 ) {
          newCSS.append( css.mid( currentPos, semicolonPos - currentPos + 1 ) );
          currentPos = semicolonPos + 1;
          continue;
        }
      }

      // Skip @page rules as GoldenDict uses its own page layout
      if ( ruleName.compare( "@page", Qt::CaseInsensitive ) == 0 ) {
        int closeBracePos = findMatchingBracket( css, currentPos );
        if ( closeBracePos != -1 ) {
          currentPos = closeBracePos + 1;
          continue;
        }
      }

      // Find block start and semicolon positions for different @ rule formats
      int openBracePos = css.indexOf( '{', currentPos );
      int semicolonPos = css.indexOf( ';', currentPos );

      if ( openBracePos != -1 && ( semicolonPos == -1 || openBracePos < semicolonPos ) ) {
        // @xxx { ... } format rules (supports @media, @keyframes, @font-face, etc.)
        // Add the @rule and opening brace
        newCSS.append( css.mid( currentPos, openBracePos - currentPos + 1 ) );

        // Find matching closing brace for the @rule block
        int closeBracePos = findMatchingBracket( css, openBracePos );
        if ( closeBracePos != -1 ) {
          // Process content inside the block recursively
          QString innerCSS = css.mid( openBracePos + 1, closeBracePos - openBracePos - 1 );
          isolateCSS( innerCSS, wrapperSelector ); // Recursive call to process inner CSS
          newCSS.append( innerCSS );
          newCSS.append( '}' );
          currentPos = closeBracePos + 1;
        }
        else {
          // If no matching brace found, continue processing
          currentPos = openBracePos + 1;
        }
      }
      else if ( semicolonPos != -1 ) {
        // @xxx yyyy; format rules (supports @import, @charset, @namespace, etc.)
        newCSS.append( css.mid( currentPos, semicolonPos - currentPos + 1 ) );
        currentPos = semicolonPos + 1;
        continue;
      }
      else {
        // Unrecognized @ rule format, copy remaining content as-is
        newCSS.append( css.mid( currentPos ) );
        break;
      }
    }
    else if ( ch == '{' ) {
      // Selector declaration block - copy as is up to closing brace
      int closeBracePos = findMatchingBracket( css, currentPos );
      if ( closeBracePos != -1 ) {
        newCSS.append( css.mid( currentPos, closeBracePos - currentPos + 1 ) );
        currentPos = closeBracePos + 1;
        continue;
      }
      else {
        newCSS.append( css.mid( currentPos ) );
        break;
      }
    }
    else if ( ch.isLetter() || ch == '.' || ch == '#' || ch == '*' || ch == '\\' || ch == ':' || ch == '[' ) {
      if ( ch.isLetter() || ch == '*' ) {
        // Check for namespace prefix
        QChar chr;
        for ( int i = currentPos; i < css.length(); i++ ) {
          chr = css.at( i );
          if ( chr.isLetterOrNumber() || chr.isMark() || chr == '_' || chr == '-'
               || ( chr == '*' && i == currentPos ) ) {
            continue;
          }

          if ( chr == '|' ) {
            // Found namespace prefix, copy as is
            newCSS.append( css.mid( currentPos, i - currentPos + 1 ) );
            currentPos = i + 1;
          }
          break;
        }
        if ( chr == '|' ) {
          continue;
        }
      }

      // Handle attribute selectors specifically [attr=value]
      if ( ch == '[' ) {
        // Find the matching closing bracket for the attribute selector
        int bracketDepth      = 1;
        int closingBracketPos = -1;
        for ( int i = currentPos + 1; i < css.length(); i++ ) {
          QChar currentChar = css.at( i );
          // Skip escaped brackets and other characters
          if ( currentChar == '\\' && i + 1 < css.length() ) {
            i++; // Skip the next character as it's escaped
            continue;
          }
          if ( currentChar == '[' ) {
            bracketDepth++;
          }
          else if ( currentChar == ']' ) {
            bracketDepth--;
            if ( bracketDepth == 0 ) {
              closingBracketPos = i;
              break;
            }
          }
        }

        if ( closingBracketPos != -1 ) {
          // Add isolation prefix for attribute selectors
          newCSS.append( prefix + " " );
          newCSS.append( css.mid( currentPos, closingBracketPos - currentPos + 1 ) );
          currentPos = closingBracketPos + 1;
          continue;
        }
      }

      // Handle double colon pseudo-elements like ::before, ::after, and ::highlight(name)
      // This is a selector - add isolation prefix to ensure CSS only affects content from this dictionary
      int selectorEndPos = css.indexOf( selectorSeparatorRegex, currentPos + 1 );
      // Fix for handling complex selectors with combinators like >, +, ~
      if ( selectorEndPos < 0 ) {
        selectorEndPos = css.indexOf( selectorEndRegex, currentPos );
      }
      QString selectorPart = css.mid( currentPos, selectorEndPos < 0 ? selectorEndPos : selectorEndPos - currentPos );

      if ( selectorEndPos < 0 ) {
        newCSS.append( prefix + " " + selectorPart );
        break;
      }

      QString trimmedSelector = selectorPart.trimmed();
      if ( trimmedSelector.compare( "body", Qt::CaseInsensitive ) == 0
           || trimmedSelector.compare( "html", Qt::CaseInsensitive ) == 0 ) {
        // Special handling for body and html selectors to maintain CSS specificity
        newCSS.append( selectorPart + " " + prefix + " " );
        currentPos += trimmedSelector.length();
      }
      else {
        // Add isolation prefix to normal selectors to scope them to this dictionary's content
        newCSS.append( prefix + " " );
      }

      int ruleStartPos      = css.indexOf( selectorEndRegex, currentPos );
      QString remainingPart = css.mid( currentPos, ruleStartPos < 0 ? ruleStartPos : ruleStartPos - currentPos );
      newCSS.append( remainingPart );

      if ( ruleStartPos < 0 ) {
        break;
      }

      currentPos = ruleStartPos;
      continue;
    }
    else {
      newCSS.append( ch );
      currentPos++;
    }
  }

  css = newCSS;
}

// Helper function to find matching closing bracket considering nesting and escaped characters
int findMatchingBracket( const QString & css, int startPos )
{
  int depth = 1;
  for ( int i = startPos + 1; i < css.length(); i++ ) {
    QChar ch = css.at( i );

    // Skip escaped characters
    if ( ch == '\\' && i + 1 < css.length() ) {
      i++;
      continue;
    }

    if ( ch == '{' ) {
      depth++;
    }
    else if ( ch == '}' ) {
      depth--;
      if ( depth == 0 ) {
        return i;
      }
    }
  }
  return -1; // No matching bracket found
}

string makeDictionaryId( const vector< string > & dictionaryFiles ) noexcept
{
  std::vector< string > sortedList;

  if ( Config::isPortableVersion() ) {
    // For portable version, we use relative paths
    sortedList.reserve( dictionaryFiles.size() );

    const QDir dictionariesDir( Config::getPortableVersionDictionaryDir() );

    for ( const auto & full : dictionaryFiles ) {
      QFileInfo fileInfo( QString::fromStdString( full ) );

      if ( fileInfo.isAbsolute() ) {
        sortedList.push_back( dictionariesDir.relativeFilePath( fileInfo.filePath() ).toStdString() );
      }
      else {
        // Well, it's relative. We don't technically support those, but
        // what the heck
        sortedList.push_back( full );
      }
    }
  }
  else {
    sortedList = dictionaryFiles;
  }

  std::sort( sortedList.begin(), sortedList.end() );

  QCryptographicHash hash( QCryptographicHash::Md5 );

  for ( const auto & i : sortedList ) {
    // Note: a null byte at the end is a must
    hash.addData( { i.c_str(), static_cast< qsizetype >( i.size() + 1 ) } );
  }

  return hash.result().toHex().data();
}

// While this file is not supposed to have any Qt stuff since it's used by
// the dictionary backends, there's no platform-independent way to get hold
// of a timestamp of the file, so we use here Qt anyway. It is supposed to
// be fixed in the future when it's needed.
bool needToRebuildIndex( const vector< string > & dictionaryFiles, const string & indexFile ) noexcept
{
  // First check if the dictionary is scheduled for reindexing
  std::string dictId = makeDictionaryId( dictionaryFiles );
  Config::Class cfg = Config::load();
  
  // If the dictionary ID is in the reindex list, return true and remove it
  if ( cfg.dictionariesToReindex.contains( QString::fromStdString( dictId ) ) ) {
    // Remove immediately to ensure index is rebuilt only once
    cfg.dictionariesToReindex.remove( QString::fromStdString( dictId ) );

    return true;
  }
  
  // Original logic: check file modification times
  qint64 lastModified = 0;

  for ( const auto & dictionaryFile : dictionaryFiles ) {
    QString name = QString::fromUtf8( dictionaryFile.c_str() );
    QFileInfo fileInfo( name );
    qint64 ts;

    if ( fileInfo.isDir() ) {
      continue;
    }

    if ( name.toLower().endsWith( ".zip" ) ) {
      ZipFile::SplitZipFile zf( name );
      if ( !zf.exists() ) {
        return true;
      }
      ts = zf.lastModified().toSecsSinceEpoch();
    }
    else {
      if ( !fileInfo.exists() ) {
        continue;
      }
      ts = fileInfo.lastModified().toSecsSinceEpoch();
    }

    if ( ts > lastModified ) {
      lastModified = ts;
    }
  }


  QFileInfo fileInfo( indexFile.c_str() );

  if ( !fileInfo.exists() ) {
    return true;
  }

  return fileInfo.lastModified().toSecsSinceEpoch() < lastModified;
}

string getFtsSuffix()
{
  return "_FTS_x";
}

QString generateRandomDictionaryId()
{
  return QCryptographicHash::hash( QUuid::createUuid().toString().toUtf8(), QCryptographicHash::Md5 ).toHex();
}

QMap< std::string, sptr< Dictionary::Class > > dictToMap( const std::vector< sptr< Dictionary::Class > > & dicts )
{
  QMap< std::string, sptr< Dictionary::Class > > dictMap;
  for ( const auto & dict : dicts ) {
    if ( !dict ) {
      continue;
    }
    dictMap.insert( dict.get()->getId(), dict );
  }
  return dictMap;
}
} // namespace Dictionary
