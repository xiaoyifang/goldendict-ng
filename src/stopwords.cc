#include "stopwords.hh"
#include "config.hh"
#include <QDebug>
#include <QFile>
#include <QSet>
#include <QTextStream>

namespace Stopwords {

std::vector< std::string > getStopwords()
{
  // Use QSet to automatically handle duplicate stopwords
  QSet< QString > stopwordsSet;

  // Load built-in stopwords from the resource file
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

  // Try to load and merge user-defined stopwords from config directory
  // Location:
  //   Windows: %APPDATA%\GoldenDict\stopwords.txt
  //   Linux/Unix: ~/.goldendict/stopwords.txt or ~/.config/goldendict/stopwords.txt
  //   macOS: ~/.goldendict/stopwords.txt
  //   Portable: <program_directory>/portable/stopwords.txt
  //
  // Users can:
  //   1. Add custom stopwords: simply list words (one per line)
  //   2. Remove built-in stopwords: prefix with minus sign (e.g., "-the" to remove "the")
  QString stopwordsFile = Config::getHomeDir().filePath( "stopwords.txt" );
  QFile customStopwords( stopwordsFile );
  if ( customStopwords.exists() && customStopwords.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
    qDebug() << "Loading custom stopwords from" << stopwordsFile;
    QTextStream in( &customStopwords );
    while ( !in.atEnd() ) {
      QString line = in.readLine().trimmed();
      if ( !line.isEmpty() && !line.startsWith( '#' ) ) {
        // Check if this is a removal instruction (starts with '-')
        if ( line.startsWith( '-' ) && line.length() > 1 ) {
          QString wordToRemove = line.mid( 1 ).trimmed();
          if ( !wordToRemove.isEmpty() ) {
            stopwordsSet.remove( wordToRemove );
            qDebug() << "Removed stopword:" << wordToRemove;
          }
        }
        else {
          // Add new stopword
          stopwordsSet.insert( line );
        }
      }
    }
    customStopwords.close();
  }

  // Convert QSet to std::vector<std::string>
  std::vector< std::string > result;
  result.reserve( stopwordsSet.size() );
  for ( const auto & word : std::as_const( stopwordsSet ) ) {
    result.push_back( word.toStdString() );
  }

  return result;
}

} // namespace Stopwords
