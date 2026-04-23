#include "stopwords.hh"
#include "config.hh"
#include <QDebug>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <mutex>
#include "xapian.h"

namespace Stopwords {

namespace {
std::vector< std::string > cachedStopwords;
std::once_flag stopwordsLoadedFlag;
Xapian::SimpleStopper * cachedStopper = nullptr;
std::once_flag stopperCreatedFlag;

void loadStopwords()
{
  QSet< QString > stopwordsSet;

  QFile bundledStopwords( ":/src/data/stopwords.txt" );
  if ( bundledStopwords.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    QTextStream in( &bundledStopwords );
    while ( !in.atEnd() ) {
      QString line = in.readLine().trimmed();
      if ( !line.isEmpty() && !line.startsWith( '#' ) ) {
        stopwordsSet.insert( line );
      }
    }
    bundledStopwords.close();
  }

  QString stopwordsFile = Config::getHomeDir().filePath( "stopwords.txt" );
  QFile customStopwords( stopwordsFile );
  if ( customStopwords.exists() && customStopwords.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    qDebug() << "Loading custom stopwords from" << stopwordsFile;
    QTextStream in( &customStopwords );
    while ( !in.atEnd() ) {
      QString line = in.readLine().trimmed();
      if ( !line.isEmpty() && !line.startsWith( '#' ) ) {
        if ( line.startsWith( '-' ) && line.length() > 1 ) {
          QString wordToRemove = line.mid( 1 ).trimmed();
          if ( !wordToRemove.isEmpty() ) {
            stopwordsSet.remove( wordToRemove );
            qDebug() << "Removed stopword:" << wordToRemove;
          }
        }
        else {
          stopwordsSet.insert( line );
        }
      }
    }
    customStopwords.close();
  }

  cachedStopwords.reserve( stopwordsSet.size() );
  for ( const auto & word : std::as_const( stopwordsSet ) ) {
    cachedStopwords.push_back( word.toStdString() );
  }
}

void createStopper()
{
  std::call_once( stopwordsLoadedFlag, loadStopwords );
  if ( !cachedStopwords.empty() ) {
    cachedStopper = new Xapian::SimpleStopper();
    for ( const auto & word : cachedStopwords ) {
      cachedStopper->add( word );
    }
    qDebug() << "Stopwords: Created cached stopper with" << cachedStopwords.size() << "stopwords";
  }
}
} // namespace

std::vector< std::string > getStopwords()
{
  std::call_once( stopwordsLoadedFlag, loadStopwords );
  return cachedStopwords;
}

Xapian::SimpleStopper * getStopper()
{
  std::call_once( stopperCreatedFlag, createStopper );
  return cachedStopper;
}

} // namespace Stopwords
