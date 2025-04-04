/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "lsa.hh"
#include "dictfile.hh"
#include "iconv.hh"
#include "folding.hh"
#include "text.hh"
#include "btreeidx.hh"

#include "audiolink.hh"

#include <set>
#include <string>

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>
#include <QDir>
#include <QUrl>
#include <QFile>

#include "utils.hh"

namespace Lsa {

using std::string;
using std::map;
using std::multimap;
using std::set;
using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

DEF_EX( exInvalidData, "Invalid data encountered", Dictionary::Ex )
DEF_EX( exFailedToOpenVorbisData, "Failed to open Vorbis data", Dictionary::Ex )
DEF_EX( exFailedToSeekInVorbisData, "Failed to seek in Vorbis data", Dictionary::Ex )
DEF_EX( exFailedToRetrieveVorbisInfo, "Failed to retrieve Vorbis info", Dictionary::Ex )

enum {
  Signature            = 0x5841534c, // LSAX on little-endian, XASL on big-endian
  CurrentFormatVersion = 5
};

#pragma pack( push, 1 )

struct IdxHeader
{
  uint32_t signature;             // First comes the signature, BGLX
  uint32_t formatVersion;         // File format version, currently 1.
  uint32_t soundsCount;           // Total number of sounds, for informative purposes only
  uint32_t vorbisOffset;          // Offset of the vorbis file which contains all snds
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

bool indexIsOldOrBad( string const & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}

string stripExtension( string const & str )
{
  if ( Utils::endsWithIgnoreCase( str, ".wav" ) ) {
    return string( str, 0, str.size() - 4 );
  }
  else {
    return str;
  }
}

struct Entry
{
  string name;

  uint32_t samplesLength;
  uint32_t samplesOffset;

public:

  // Reads an entry from the file's current position
  Entry( File::Index & f );
};

Entry::Entry( File::Index & f )
{
  bool firstEntry = ( f.tell() == 13 );
  // Read the entry's filename
  size_t read = 0;

  vector< uint16_t > filenameBuffer( 64 );

  for ( ;; ++read ) {
    if ( filenameBuffer.size() <= read ) {
      filenameBuffer.resize( read + 64 );
    }

    f.read( &filenameBuffer[ read ], 2 );

    if ( filenameBuffer[ read ] == 0xD ) {
      if ( f.read< uint16_t >() != 0xA ) {
        throw exInvalidData();
      }

      // Filename ending marker
      break;
    }
  }

  // Skip zero or ff, or just ff.

  if ( auto x = f.read< uint8_t >() ) {
    if ( x != 0xFF ) {
      throw exInvalidData();
    }
  }
  else if ( f.read< uint8_t >() != 0xFF ) {
    throw exInvalidData();
  }


  if ( !firstEntry ) {
    // For all entries but the first one, read its offset in
    // samples.
    samplesOffset = f.read< uint32_t >();

    if ( f.read< uint8_t >() != 0xFF ) {
      throw exInvalidData();
    }
  }
  else {
    samplesOffset = 0;
  }

  // Read the size of the recording, in samples
  samplesLength = f.read< uint32_t >();

  name = Iconv::toUtf8( Text::utf16_le, &filenameBuffer.front(), read * sizeof( uint16_t ) );
}

class LsaDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;

public:

  LsaDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles );

  string getName() noexcept override;

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.soundsCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return getArticleCount();
  }

  sptr< Dictionary::DataRequest > getArticle( std::u32string const &,
                                              vector< std::u32string > const & alts,
                                              std::u32string const &,
                                              bool ignoreDiacritics ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

protected:

  void loadIcon() noexcept override;
};

string LsaDictionary::getName() noexcept
{
  string result = Utils::Fs::basename( getDictionaryFilenames()[ 0 ] );

  // Strip the extension
  result.erase( result.rfind( '.' ) );

  return result;
}

LsaDictionary::LsaDictionary( string const & id, string const & indexFile, vector< string > const & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly ),
  idxHeader( idx.read< IdxHeader >() )
{
  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );
}

sptr< Dictionary::DataRequest > LsaDictionary::getArticle( std::u32string const & word,
                                                           vector< std::u32string > const & alts,
                                                           std::u32string const &,
                                                           bool ignoreDiacritics )

{
  vector< WordArticleLink > chain = findArticles( word, ignoreDiacritics );

  for ( const auto & alt : alts ) {
    /// Make an additional query for each alt

    vector< WordArticleLink > altChain = findArticles( alt, ignoreDiacritics );

    chain.insert( chain.end(), altChain.begin(), altChain.end() );
  }

  multimap< std::u32string, string > mainArticles, alternateArticles;

  set< uint32_t > articlesIncluded; // Some synonims make it that the articles
                                    // appear several times. We combat this
                                    // by only allowing them to appear once.

  std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
  if ( ignoreDiacritics ) {
    wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
  }

  for ( auto & x : chain ) {
    if ( articlesIncluded.find( x.articleOffset ) != articlesIncluded.end() ) {
      continue; // We already have this article in the body.
    }

    // Ok. Now, does it go to main articles, or to alternate ones? We list
    // main ones first, and alternates after.

    // We do the case-folded comparison here.

    std::u32string headwordStripped = Folding::applySimpleCaseOnly( x.word );
    if ( ignoreDiacritics ) {
      headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
    }

    multimap< std::u32string, string > & mapToUse =
      ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

    mapToUse.insert( std::pair( Folding::applySimpleCaseOnly( x.word ), x.word ) );

    articlesIncluded.insert( x.articleOffset );
  }

  if ( mainArticles.empty() && alternateArticles.empty() ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such word
  }

  string result;

  multimap< std::u32string, string >::const_iterator i;

  result += "<div class=\"audio-play\">";
  for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
    result += "<div class=\"audio-play-item\">";

    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( i->second.c_str() ) ) );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
    result += "<a href=" + ref + ">" + i->second + "</a>";
    result += "</div>";
  }

  for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
    result += "<div class=\"audio-play-item\">";

    QUrl url;
    url.setScheme( "gdau" );
    url.setHost( QString::fromUtf8( getId().c_str() ) );
    url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( i->second.c_str() ) ) );

    string ref = string( "\"" ) + url.toEncoded().data() + "\"";

    result += addAudioLink( url.toEncoded(), getId() );

    result += "<a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a>)";
    result += "<a href=" + ref + ">" + i->second + "</a>";
    result += "</div>";
  }

  result += "</div>";

  auto * ret = new Dictionary::DataRequestInstant( true );

  ret->getData().resize( result.size() );

  memcpy( &( ret->getData().front() ), result.data(), result.size() );

  return std::shared_ptr< Dictionary::DataRequestInstant >( ret );
}

/// This wraps around file operations
struct ShiftedVorbis
{
  QFile & f;
  size_t shift;

  ShiftedVorbis( QFile & f_, size_t shift_ ):
    f( f_ ),
    shift( shift_ )
  {
  }

  static size_t read( void * ptr, size_t size, size_t nmemb, void * datasource );
  static int seek( void * datasource, ogg_int64_t offset, int whence );
  static long tell( void * datasource );

  static ov_callbacks callbacks;
};

size_t ShiftedVorbis::read( void * ptr, size_t size, size_t nmemb, void * datasource )
{
  auto * sv = (ShiftedVorbis *)datasource;

  return sv->f.read( reinterpret_cast< char * >( ptr ), size * nmemb );
}

int ShiftedVorbis::seek( void * datasource, ogg_int64_t offset, int whence )
{
  auto * sv = (ShiftedVorbis *)datasource;

  if ( whence == SEEK_SET ) {
    offset += sv->shift;
  }

  if ( whence == SEEK_CUR ) {
    offset += sv->f.pos();
  }

  if ( whence == SEEK_END ) {
    offset += sv->f.size();
  }

  return sv->f.seek( offset );
}

long ShiftedVorbis::tell( void * datasource )
{
  auto * sv   = (ShiftedVorbis *)datasource;
  long result = sv->f.pos();

  if ( result != -1 ) {
    result -= sv->shift;
  }

  return result;
}

ov_callbacks ShiftedVorbis::callbacks = { ShiftedVorbis::read, ShiftedVorbis::seek, NULL, ShiftedVorbis::tell };

// A crude .wav header which is sufficient for our needs
struct WavHeader
{
  char riff[ 4 ]; // RIFF
  uint32_t riffLength;
  char waveAndFmt[ 8 ]; // WAVEfmt%20
  uint32_t fmtLength;   // 16
  uint16_t formatTag;   // 1
  uint16_t channels;    // 1 or 2
  uint32_t samplesPerSec;
  uint32_t bytesPerSec;
  uint16_t blockAlign;
  uint16_t bitsPerSample; // 16
  char data[ 4 ];         // data
  uint32_t dataLength;
}
#ifndef _MSC_VER
__attribute__( ( packed ) )
#endif
;

sptr< Dictionary::DataRequest > LsaDictionary::getResource( string const & name )

{
  // See if the name ends in .wav. Remove that extension then

  string strippedName = Utils::endsWithIgnoreCase( name, ".wav" ) ? string( name, 0, name.size() - 4 ) : name;

  vector< WordArticleLink > chain = findArticles( Text::toUtf32( strippedName ) );

  if ( chain.empty() ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false ); // No such resource
  }

  File::Index f( getDictionaryFilenames()[ 0 ], QIODevice::ReadOnly );

  f.seek( chain[ 0 ].articleOffset );
  Entry e( f );

  f.seek( idxHeader.vorbisOffset );

  ShiftedVorbis sv( f.file(), idxHeader.vorbisOffset );

  OggVorbis_File vf;

  int result = ov_open_callbacks( &sv, &vf, 0, 0, ShiftedVorbis::callbacks );

  if ( result ) {
    throw exFailedToOpenVorbisData();
  }

  if ( ov_pcm_seek( &vf, e.samplesOffset ) ) {
    throw exFailedToSeekInVorbisData();
  }

  vorbis_info * vi = ov_info( &vf, -1 );

  if ( !vi ) {
    ov_clear( &vf );

    throw exFailedToRetrieveVorbisInfo();
  }

  sptr< Dictionary::DataRequestInstant > dr = std::make_shared< Dictionary::DataRequestInstant >( true );

  vector< char > & data = dr->getData();

  data.resize( sizeof( WavHeader ) + e.samplesLength * 2 );

  auto * wh = (WavHeader *)&data.front();

  memset( wh, 0, sizeof( *wh ) );

  memcpy( wh->riff, "RIFF", 4 );
  wh->riffLength = data.size() - 8;

  memcpy( wh->waveAndFmt, "WAVEfmt ", 8 );
  wh->fmtLength     = 16;
  wh->formatTag     = 1;
  wh->channels      = vi->channels;
  wh->samplesPerSec = vi->rate;
  wh->bytesPerSec   = vi->channels * vi->rate * 2;
  wh->blockAlign    = vi->channels * 2;
  wh->bitsPerSample = 16;
  memcpy( wh->data, "data", 4 );
  wh->dataLength = data.size() - sizeof( *wh );

  // Now decode vorbis to the rest of the block

  char * ptr    = &data.front() + sizeof( *wh );
  int left      = data.size() - sizeof( *wh );
  int bitstream = 0;

  while ( left ) {
    long result = ov_read( &vf, ptr, left, 0, 2, 1, &bitstream );

    if ( result <= 0 ) {
      qWarning( "Failed to read Vorbis data (code = %ld)", result );
      memset( ptr, 0, left );
      break;
    }

    if ( result > left ) {
      qWarning( "Warning: Vorbis decode returned more data than requested." );

      result = left;
    }

    ptr += result;
    left -= result;
  }

  ov_clear( &vf );

  return dr;
}

void LsaDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/lsasound.png" );
  }

  dictionaryIconLoaded = true;
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( vector< string > const & fileNames,
                                                      string const & indicesDir,
                                                      Dictionary::Initializing & initializing )
{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( auto i = fileNames.begin(); i != fileNames.end(); ++i ) {
    /// Only allow .dat and .lsa extensions to save scanning time
    if ( !Utils::endsWithIgnoreCase( *i, ".dat" ) && !Utils::endsWithIgnoreCase( *i, ".lsa" ) ) {
      continue;
    }

    try {
      File::Index f( *i, QIODevice::ReadOnly );

      /// Check the signature

      char buf[ 9 ];

      if ( f.readRecords( buf, 9, 1 ) != 1 || memcmp( buf, "L\09\0S\0A\0\xff", 9 ) != 0 ) {
        // The file is too short or the signature doesn't match -- skip this
        // file
        continue;
      }

      vector< string > dictFiles( 1, *i );

      string dictId = Dictionary::makeDictionaryId( dictFiles );

      string indexFile = indicesDir + dictId;

      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        // Building the index

        qDebug( "Lsa: Building the index for dictionary: %s", i->c_str() );

        initializing.indexingDictionary( Utils::Fs::basename( *i ) );

        File::Index idx( indexFile, QIODevice::WriteOnly );

        IdxHeader idxHeader;

        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        IndexedWords indexedWords;

        /// XXX handle big-endian machines here!
        auto entriesCount = f.read< uint32_t >();

        qDebug( "%s: %u entries", i->c_str(), entriesCount );

        idxHeader.soundsCount = entriesCount;

        vector< uint16_t > filenameBuffer;

        while ( entriesCount-- ) {
          uint32_t offset = f.tell();

          Entry e( f );


          // Remove the extension, no need for that in the index
          e.name = stripExtension( e.name );

          qDebug( "Read filename %s (%u at %u)<", e.name.c_str(), e.samplesLength, e.samplesOffset );

          // Insert new entry into an index

          indexedWords.addWord( Text::toUtf32( e.name ), offset );
        }

        idxHeader.vorbisOffset = f.tell();

        // Make sure there's really some vobis data there

        char buf[ 4 ];

        f.read( buf, sizeof( buf ) );

        if ( strncmp( buf, "OggS", 4 ) != 0 ) {
          throw exInvalidData();
        }

        // Build the index

        IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

        idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
        idxHeader.indexRootOffset       = idxInfo.rootOffset;

        // That concludes it. Update the header.

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }

      dictionaries.push_back( std::make_shared< LsaDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Lingvo's LSA reading failed: %s, error: %s", i->c_str(), e.what() );
    }
  }

  return dictionaries;
}


} // namespace Lsa
