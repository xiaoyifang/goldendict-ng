/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "stardict.hh"
#include "btreeidx.hh"
#include "folding.hh"
#include "text.hh"
#include "chunkedstorage.hh"
#include "dictzip.hh"
#include "xdxf2html.hh"
#include "htmlescape.hh"
#include "langcoder.hh"
#include "filetype.hh"
#include "indexedzip.hh"
#include "tiff.hh"
#include "ftshelpers.hh"
#include "audiolink.hh"
#include <zlib.h>
#include <map>
#include <set>
#include <string>
#include <QString>
#include <QAtomicInt>
#include <QDomDocument>
#include "ufile.hh"
#include "utils.hh"
#include <charconv>
#include <QRegularExpression>
#include "globalregex.hh"
#include <QDir>

#ifndef Q_OS_WIN
  #include <arpa/inet.h>
#else
  #include <winsock.h>
#endif

namespace Stardict {

using std::map;
using std::multimap;
using std::pair;
using std::set;
using std::string;

using BtreeIndexing::WordArticleLink;
using BtreeIndexing::IndexedWords;
using BtreeIndexing::IndexInfo;

namespace {

DEF_EX( exNotAnIfoFile, "Not an .ifo file", Dictionary::Ex )
DEF_EX_STR( exBadFieldInIfo, "Bad field in .ifo file encountered:", Dictionary::Ex )
DEF_EX_STR( exNoIdxFile, "No corresponding .idx file was found for", Dictionary::Ex )
DEF_EX_STR( exNoDictFile, "No corresponding .dict file was found for", Dictionary::Ex )

DEF_EX( ex64BitsNotSupported, "64-bit indices are not presently supported, sorry", Dictionary::Ex )
DEF_EX( exDicttypeNotSupported, "Dictionaries with dicttypes are not supported, sorry", Dictionary::Ex )

using Dictionary::exCantReadFile;
DEF_EX_STR( exDictzipError, "DICTZIP error", Dictionary::Ex )

DEF_EX_STR( exIncorrectOffset, "Incorrect offset encountered in file", Dictionary::Ex )

/// Contents of an ifo file
struct Ifo
{
  string bookname;
  uint32_t wordcount     = 0;
  uint32_t synwordcount  = 0;
  uint32_t idxfilesize   = 0;
  uint32_t idxoffsetbits = 32;
  string sametypesequence, dicttype, description;
  string copyright, author, email, website, date;

  explicit Ifo( const QString & fileName );
};

enum {
  Signature            = 0x58444953, // SIDX on little-endian, XDIS on big-endian
  CurrentFormatVersion = 9 + BtreeIndexing::FormatVersion + Folding::Version + BtreeIndexing::ZipParseLogicVersion
};

#pragma pack( push, 1 )
struct IdxHeader
{
  uint32_t signature;             // First comes the signature, SIDX
  uint32_t formatVersion;         // File format version (CurrentFormatVersion)
  uint32_t chunksOffset;          // The offset to chunks' storage
  uint32_t indexBtreeMaxElements; // Two fields from IndexInfo
  uint32_t indexRootOffset;
  uint32_t wordCount;                // Saved from Ifo::wordcount
  uint32_t synWordCount;             // Saved from Ifo::synwordcount
  uint32_t bookNameSize;             // Book name's length. Used to read it then.
  uint32_t sameTypeSequenceSize;     // That string's size. Used to read it then.
  uint32_t langFrom;                 // Source language
  uint32_t langTo;                   // Target language
  uint32_t hasZipFile;               // Non-zero means there's a zip file with resources present
  uint32_t zipIndexBtreeMaxElements; // Two fields from IndexInfo of the zip
                                     // resource index.
  uint32_t zipIndexRootOffset;
};
static_assert( alignof( IdxHeader ) == 1 );
#pragma pack( pop )

;

bool indexIsOldOrBad( const string & indexFile )
{
  File::Index idx( indexFile, QIODevice::ReadOnly );

  IdxHeader header;

  return idx.readRecords( &header, sizeof( header ), 1 ) != 1 || header.signature != Signature
    || header.formatVersion != CurrentFormatVersion;
}

class StardictDictionary: public BtreeIndexing::BtreeDictionary
{
  QMutex idxMutex;
  File::Index idx;
  IdxHeader idxHeader;
  string sameTypeSequence;
  std::unique_ptr< ChunkedStorage::Reader > chunks;
  QMutex dzMutex;
  dictData * dz;
  QMutex resourceZipMutex;
  IndexedZip resourceZip;

public:

  StardictDictionary( const string & id, const string & indexFile, const vector< string > & dictionaryFiles );

  ~StardictDictionary();

  unsigned long getArticleCount() noexcept override
  {
    return idxHeader.wordCount;
  }

  unsigned long getWordCount() noexcept override
  {
    return idxHeader.wordCount + idxHeader.synWordCount;
  }

  inline quint32 getLangFrom() const override
  {
    return idxHeader.langFrom;
  }

  inline quint32 getLangTo() const override
  {
    return idxHeader.langTo;
  }

  sptr< Dictionary::WordSearchRequest > findHeadwordsForSynonym( const std::u32string & ) override;

  sptr< Dictionary::DataRequest > getArticle( const std::u32string &,
                                              const vector< std::u32string > & alts,
                                              const std::u32string &,
                                              bool ignoreDiacritics ) override;

  sptr< ResourceRequest > getResource( const string & name ) override;

  const QString & getDescription() override;

  QString getMainFilename() override;

  sptr< Dictionary::DataRequest >
  getSearchResults( const QString & searchString, int searchMode, bool matchCase, bool ignoreDiacritics ) override;
  void getArticleText( uint32_t articleAddress, QString & headword, QString & text ) override;

  void makeFTSIndex( QAtomicInt & isCancelled ) override;

  void setFTSParameters( const Config::FullTextSearch & fts ) override
  {
    if ( metadata_enable_fts.has_value() ) {
      can_FTS = fts.enabled && metadata_enable_fts.value();
    }
    else {
      can_FTS = fts.enabled && !fts.disabledTypes.contains( "STARDICT", Qt::CaseInsensitive )
        && ( fts.maxDictionarySize == 0 || getArticleCount() <= fts.maxDictionarySize );
    }
  }

protected:

  void loadIcon() noexcept override;

private:

  /// Retrieves the article's offset/size in .dict file, and its headword.
  void getArticleProps( uint32_t articleAddress, string & headword, uint32_t & offset, uint32_t & size );

  /// Loads the article, storing its headword and formatting the data it has
  /// into an html.
  void loadArticle( uint32_t address, string & headword, string & articleText );

  string loadString( size_t size );

  string handleResource( char type, const char * resource, size_t size );

  void pangoToHtml( QString & text );

  friend class StardictResourceRequest;
  friend class StardictArticleRequest;
  friend class StardictHeadwordsRequest;
};

StardictDictionary::StardictDictionary( const string & id,
                                        const string & indexFile,
                                        const vector< string > & dictionaryFiles ):
  BtreeDictionary( id, dictionaryFiles ),
  idx( indexFile, QIODevice::ReadOnly )
{
  // reading headers, note that reading order matters
  idxHeader        = idx.read< IdxHeader >();
  dictionaryName   = loadString( idxHeader.bookNameSize );
  sameTypeSequence = loadString( idxHeader.sameTypeSequenceSize );
  chunks           = std::make_unique< ChunkedStorage::Reader >( idx, idxHeader.chunksOffset );

  // Open the .dict file

  DZ_ERRORS error;
  dz = dict_data_open( dictionaryFiles[ 2 ].c_str(), &error, 0 );

  if ( !dz ) {
    throw exDictzipError( string( dz_error_str( error ) ) + "(" + dictionaryFiles[ 2 ] + ")" );
  }

  // Initialize the index

  openIndex( IndexInfo( idxHeader.indexBtreeMaxElements, idxHeader.indexRootOffset ), idx, idxMutex );

  // Open a resource zip file, if there's one

  if ( idxHeader.hasZipFile && ( idxHeader.zipIndexBtreeMaxElements || idxHeader.zipIndexRootOffset ) ) {
    resourceZip.openIndex( IndexInfo( idxHeader.zipIndexBtreeMaxElements, idxHeader.zipIndexRootOffset ),
                           idx,
                           idxMutex );

    QString zipName = QDir::fromNativeSeparators( getDictionaryFilenames().back().c_str() );

    if ( zipName.endsWith( ".zip", Qt::CaseInsensitive ) ) { // Sanity check
      resourceZip.openZipFile( zipName );
    }
  }

  // Full-text search parameters

  ftsIdxName = indexFile + Dictionary::getFtsSuffix();
}

StardictDictionary::~StardictDictionary()
{
  if ( dz ) {
    dict_data_close( dz );
  }
}

void StardictDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  QString fileName = QDir::fromNativeSeparators( getDictionaryFilenames()[ 0 ].c_str() );

  if ( !loadIconFromFileName( fileName ) ) {
    // Load failed -- use default icons
    dictionaryIcon = QIcon( ":/icons/icon32_stardict.png" );
  }

  dictionaryIconLoaded = true;
}

string StardictDictionary::loadString( size_t size )
{
  if ( size == 0 ) {
    return string();
  }

  vector< char > data( size );

  idx.read( &data.front(), data.size() );

  string result( &data.front(), data.size() );
  //trim the string
  result.erase( result.find_last_not_of( " \n\r\t" ) + 1 );
  result.erase( 0, result.find_first_not_of( " \n\r\t" ) );

  return result;
}

void StardictDictionary::getArticleProps( uint32_t articleAddress,
                                          string & headword,
                                          uint32_t & offset,
                                          uint32_t & size )
{
  vector< char > chunk;

  QMutexLocker _( &idxMutex );

  char * articleData = chunks->getBlock( articleAddress, chunk );

  memcpy( &offset, articleData, sizeof( uint32_t ) );
  articleData += sizeof( uint32_t );
  memcpy( &size, articleData, sizeof( uint32_t ) );
  articleData += sizeof( uint32_t );

  headword = articleData;
}

class PowerWordDataProcessor
{
  class PWSyntaxTranslate
  {
  public:
    PWSyntaxTranslate( const char * re, const char * replacement ):
      _re( re, QRegularExpression::UseUnicodePropertiesOption )

      ,
      _replacement( replacement )
    {
    }
    const QRegularExpression & re() const
    {

      return _re;
    }
    const QString & replacement() const
    {
      return _replacement;
    }

  private:
    QRegularExpression _re;

    QString _replacement;
  };

public:
  PowerWordDataProcessor( const char * resource, size_t size ):
    _data( QString::fromUtf8( resource, size ) )
  {
  }

  string process()
  {
    QDomDocument doc;
    QString ss;
    ss = "<div class=\"sdct_k\">";
    if ( !doc.setContent( _data ) ) {
      ss += _data;
    }
    else {
      QStringList sl;
      walkNode( doc.firstChild(), sl );

      for ( auto s : std::as_const( sl ) ) {
        translatePW( s );
        ss += s;
        ss += "<br>";
      }
    }
    ss += "</div>";
    QByteArray ba = ss.toUtf8();
    return string( ba.data(), ba.size() );
  }

private:
  void walkNode( const QDomNode & e, QStringList & sl )
  {
    if ( e.isNull() ) {
      return;
    }
    if ( e.isText() ) {
      sl.append( e.toText().data() );
    }
    else {
      QDomNodeList l = e.childNodes();
      for ( int i = 0; i < l.size(); ++i ) {
        QDomNode n = l.at( i );
        if ( n.isText() ) {
          sl.append( n.toText().data() );
        }
        else {
          walkNode( n, sl );
        }
      }
    }
  }

  void translatePW( QString & s )
  {
    const int TRANSLATE_TBL_SIZE                     = 5;
    static PWSyntaxTranslate t[ TRANSLATE_TBL_SIZE ] = {
      PWSyntaxTranslate( R"(&[bB]\s*\{([^\{}&]+)\})", "<B>\\1</B>" ),
      PWSyntaxTranslate( R"(&[iI]\s*\{([^\{}&]+)\})", "<I>\\1</I>" ),
      PWSyntaxTranslate( R"(&[uU]\s*\{([^\{}&]+)\})", "<U>\\1</U>" ),
      PWSyntaxTranslate( R"(&[lL]\s*\{([^\{}&]+)\})", R"(<SPAN style="color:#0000ff">\1</SPAN>)" ),
      PWSyntaxTranslate( R"(&[2]\s*\{([^\{}&]+)\})", R"(<SPAN style="color:#0000ff">\1</SPAN>)" ) };

    QString old;
    while ( s.compare( old ) != 0 ) {
      for ( auto & a : t ) {
        s.replace( a.re(), a.replacement() );
      }
      old = s;
    }

    static QRegularExpression leadingBrace( "&.\\s*\\{",
                                            QRegularExpression::UseUnicodePropertiesOption
                                              | QRegularExpression::DotMatchesEverythingOption );

    s.replace( leadingBrace, "" );
    s.replace( "}", "" );
  }

private:
  QString _data;
};


/// This function tries to make an html of the Stardict's resource typed
/// 'type', contained in a block pointed to by 'resource', 'size' bytes long.
string StardictDictionary::handleResource( char type, const char * resource, size_t size )
{
  QString text;

  // See "Type identifiers" at http://www.huzheng.org/stardict/StarDictFileFormat
  switch ( type ) {
    case 'x': // Xdxf content
      return Xdxf2Html::convert( string( resource, size ), Xdxf2Html::STARDICT, NULL, this );
    case 'h': // Html content
    {
      QString articleText = QString( "<div class=\"sdct_h\">" ) + QString::fromUtf8( resource, size ) + "</div>";

      // Replace urls that is relative path but not direct data or https
      // Match: <link href=abc.png/>
      // Match:  <img src='abc.png'/>
      // Not Match: <link href='http://abc.png'/>
      // Not Match: <link href='data:image/jpeg;.......'/>
      static QRegularExpression imgRe(
        R"((<\s*(?:img|script)\s+[^>]*src\s*=\s*["']?)(?!(?:data|https?|ftp):)([^"'\s>]+)(["']?))",
        QRegularExpression::CaseInsensitiveOption );
      static QRegularExpression linkRe(
        R"((<\s*link\s+[^>]*href\s*=\s*["']?)(?!(?:data|https?|ftp):)([^"'\s>]+)(["']?))",
        QRegularExpression::CaseInsensitiveOption );

      articleText.replace( imgRe, QString( R"(\1bres://%1/\2\3)" ).arg( QString::fromStdString( getId() ) ) )
        .replace( linkRe, QString( R"(\1bres://%1/\2\3)" ).arg( QString::fromStdString( getId() ) ) );
      // Handle links to articles

      static QRegularExpression linksReg( R"(<a(\s*[^>]*)href\s*=\s*['"](bword://)?([^'"]+)['"])",
                                          QRegularExpression::CaseInsensitiveOption );


      int pos = 0;
      QString articleNewText;
      QRegularExpressionMatchIterator it = linksReg.globalMatch( articleText );
      while ( it.hasNext() ) {
        QRegularExpressionMatch match = it.next();
        articleNewText += articleText.mid( pos, match.capturedStart() - pos );
        pos = match.capturedEnd();

        QString link = match.captured( 3 );

        if ( link.indexOf( ':' ) < 0 ) {
          //compatible with issue #567
          //such as bword://fl&#x205;k
          if ( link.contains( RX::Html::htmlEntity ) ) {
            link = Html::unescape( link );
          }

          QString newLink;
          if ( link.indexOf( '#' ) < 0 ) {
            newLink = QString( "<a" ) + match.captured( 1 ) + "href=\"bword:" + link + "\"";
          }


          // Anchors
          if ( link.indexOf( '#' ) > 0 && link.indexOf( "&#" ) < 0 ) {
            newLink = QString( "<a" ) + match.captured( 1 ) + "href=\"gdlookup://localhost/" + link + "\"";

            newLink.replace( "#", "?gdanchor=" );
          }

          if ( !newLink.isEmpty() ) {
            articleNewText += newLink;
          }
          else {
            articleNewText += match.captured();
          }
        }
        else {
          articleNewText += match.captured();
        }
      }
      if ( pos ) {
        articleNewText += articleText.mid( pos );
        articleText = articleNewText;
        articleNewText.clear();
      }

      // Handle "audio" tags

      static QRegularExpression audioRe( R"(<\s*audio\s*src\s*=\s*(["']+)([^"']+)(["'])\s*>(.*)</audio>)",
                                         QRegularExpression::CaseInsensitiveOption
                                           | QRegularExpression::DotMatchesEverythingOption );


      pos = 0;

      it = audioRe.globalMatch( articleText );
      while ( it.hasNext() ) {
        QRegularExpressionMatch match = it.next();
        articleNewText += articleText.mid( pos, match.capturedStart() - pos );
        pos = match.capturedEnd();

        QString src = match.captured( 2 );

        if ( src.indexOf( "://" ) >= 0 ) {
          articleNewText += match.captured();
        }
        else {
          std::string audioLink = "gdau://" + getId() + "/" + src.toUtf8().data();
          std::string href      = "\"" + audioLink + "\"";
          std::string newTag = addAudioLink( audioLink, getId() ) + "<span class=\"sdict_h_wav\"><a href=" + href + ">";
          newTag += match.captured( 4 ).toUtf8().constData();
          if ( match.captured( 4 ).indexOf( "<img " ) < 0 ) {
            newTag += R"( <img src="qrc:///icons/playsound.png" border="0" alt="Play">)";
          }
          newTag += "</a></span>";

          articleNewText += QString::fromStdString( newTag );
        }
      }
      if ( pos ) {
        articleNewText += articleText.mid( pos );
        articleText = articleNewText;
        articleNewText.clear();
      }
      auto text = articleText.toUtf8();
      return text.data();
    }
    case 'm': // Pure meaning, usually means preformatted text
      return "<div class=\"sdct_m\">" + Html::preformat( string( resource, size ), isToLanguageRTL() ) + "</div>";
    case 'l': // Same as 'm', but not in utf8, instead in current locale's
              // encoding.
              // We just use Qt here, it should know better about system's
              // locale.
      return "<div class=\"sdct_l\">"
        + Html::preformat( QString::fromLocal8Bit( resource, size ).toUtf8().data(), isToLanguageRTL() ) + "</div>";
    case 'g': // Pango markup.
      text = QString::fromUtf8( resource, size );
      pangoToHtml( text );
      return "<div class=\"sdct_g\">" + string( text.toUtf8().data() ) + "</div>";
    case 't': // Transcription
      return "<div class=\"sdct_t\">" + Html::escape( string( resource, size ) ) + "</div>";
    case 'y': // Chinese YinBiao or Japanese KANA. Examples are needed. For now,
              // just output as pure escaped utf8.
      return "<div class=\"sdct_y\">" + Html::escape( string( resource, size ) ) + "</div>";
    case 'k': // KingSoft PowerWord data.
    {
      PowerWordDataProcessor pwdp( resource, size );
      return pwdp.process();
    }
    case 'w': // MediaWiki markup. We don't handle this right now.
      return "<div class=\"sdct_w\">" + Html::escape( string( resource, size ) ) + "</div>";
    case 'n': // WordNet data. We don't know anything about it.
      return "<div class=\"sdct_n\">" + Html::escape( string( resource, size ) ) + "</div>";

    case 'r': // Resource file list.
    {
      string result = R"(<div class="sdct_r">)";

      // Resource file list.
      // The content can be:
      // img:pic/example.jpg	// Image file
      // snd:apple.wav		// Sound file
      // vdo:film.avi		// Video file
      // att:file.bin		// Attachment file

      // Extract the part after the prefix
      static const QMap< QString, QString > prefixTemplates = {
        { QString( "img:" ), QString( R"(<img src="bres://%1/%2"/>)" ) },
        { QString( "snd:" ), QString( R"(<audio controls src="bres://%1/%2"></audio>)" ) },
        { QString( "vdo:" ), QString( R"(<video controls src="bres://%1/%2"></video>)" ) },
        { QString( "att:" ), QString( R"(<a download href="bres://%1/%2">%1</a>)" ) } };

      for ( const auto & file : QString::fromUtf8( resource, size ).simplified().split( " " ) ) {
        if ( prefixTemplates.contains( file.left( 4 ) ) ) {
          result +=
            prefixTemplates[ file.left( 4 ) ].arg( QString::fromStdString( getId() ), file.mid( 4 ) ).toStdString();
        }
        else {
          result += Html::escape( file.toStdString() );
        }
      }

      return result + "</div>";
    }

    case 'W': // An embedded Wav file. Unhandled yet.
      return "<div class=\"sdct_W\">(an embedded .wav file)</div>";
    case 'P': // An embedded picture file. Unhandled yet.
      return "<div class=\"sdct_P\">(an embedded picture file)</div>";
  }

  if ( islower( type ) ) {
    return string( "<b>Unknown textual entry type " ) + string( 1, type ) + ":</b> "
      + Html::escape( string( resource, size ) ) + "<br>";
  }
  else {
    return string( "<b>Unknown blob entry type " ) + string( 1, type ) + "</b><br>";
  }
}

void StardictDictionary::pangoToHtml( QString & text )
{
  /*
 * Partially support for Pango Markup Language
 * Attributes "fallback", "lang", "gravity", "gravity_hint" just ignored
 */

  static QRegularExpression spanRegex( "<span\\s*([^>]*)>", QRegularExpression::CaseInsensitiveOption );
  static QRegularExpression styleRegex( "(\\w+)=\"([^\"]*)\"" );

  text.replace( "\n", "<br>" );

  int pos = 0;
  do {
    auto match = spanRegex.match( text, pos );
    pos        = match.capturedStart();
    if ( pos >= 0 ) {
      QString styles = match.captured( 1 );
      QString newSpan( "<span style=\"" );
      int stylePos = 0;
      do {
        auto styleMatch = styleRegex.match( styles, stylePos );

        stylePos      = styleMatch.capturedStart();
        QString style = styleMatch.captured( 1 );
        if ( stylePos >= 0 ) {
          auto cap2 = styleMatch.captured( 2 );

          if ( style.compare( "font_desc", Qt::CaseInsensitive ) == 0
               || style.compare( "font", Qt::CaseInsensitive ) == 0 ) {
            // Parse font description

            QStringList list = styleMatch.captured( 2 ).split( " ", Qt::SkipEmptyParts );
            int n;
            QString sizeStr, stylesStr, familiesStr;
            for ( n = list.size() - 1; n >= 0; n-- ) {
              QString str = list.at( n );

              // font size
              if ( str[ 0 ].isNumber() ) {
                sizeStr = QString( "font-size:" ) + str + ";";
                continue;
              }

              // font style
              if ( str.compare( "normal", Qt::CaseInsensitive ) == 0
                   || str.compare( "oblique", Qt::CaseInsensitive ) == 0
                   || str.compare( "italic", Qt::CaseInsensitive ) == 0 ) {
                if ( !stylesStr.contains( "font-style:" ) ) {
                  stylesStr += QString( "font-style:" ) + str + ";";
                }
                continue;
              }

              // font variant
              if ( str.compare( "smallcaps", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-variant:small-caps" );
                continue;
              }

              // font weight
              if ( str.compare( "ultralight", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-weight:100;" );
                continue;
              }
              if ( str.compare( "light", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-weight:200;" );
                continue;
              }
              if ( str.compare( "bold", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-weight:bold;" );
                continue;
              }
              if ( str.compare( "ultrabold", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-weight:800;" );
                continue;
              }
              if ( str.compare( "heavy", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-weight:900" );
                continue;
              }

              // font stretch
              if ( str.compare( "ultracondensed", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:ultra-condensed;" );
                continue;
              }
              if ( str.compare( "extracondensed", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:extra-condensed;" );
                continue;
              }
              if ( str.compare( "semicondensed", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:semi-condensed;" );
                continue;
              }
              if ( str.compare( "semiexpanded", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:semi-expanded;" );
                continue;
              }
              if ( str.compare( "extraexpanded", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:extra-expanded;" );
                continue;
              }
              if ( str.compare( "ultraexpanded", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:ultra-expanded;" );
                continue;
              }
              if ( str.compare( "condensed", Qt::CaseInsensitive ) == 0
                   || str.compare( "expanded", Qt::CaseInsensitive ) == 0 ) {
                stylesStr += QString( "font-stretch:" ) + str + ";";
                continue;
              }

              // gravity
              if ( str.compare( "south", Qt::CaseInsensitive ) == 0 || str.compare( "east", Qt::CaseInsensitive ) == 0
                   || str.compare( "north", Qt::CaseInsensitive ) == 0
                   || str.compare( "west", Qt::CaseInsensitive ) == 0
                   || str.compare( "auto", Qt::CaseInsensitive ) == 0 ) {
                continue;
              }
              break;
            }

            // last words is families list
            if ( n >= 0 ) {
              familiesStr = QString( "font-family:" );
              for ( int i = 0; i <= n; i++ ) {
                if ( i > 0 && !familiesStr.endsWith( ',' ) ) {
                  familiesStr += ",";
                }
                familiesStr += list.at( i );
              }
              familiesStr += ";";
            }

            newSpan += familiesStr + stylesStr + sizeStr;
          }
          else if ( style.compare( "font_family", Qt::CaseInsensitive ) == 0
                    || style.compare( "face", Qt::CaseInsensitive ) == 0 ) {
            newSpan += QString( "font-family:" ) + cap2 + ";";
          }
          else if ( style.compare( "font_size", Qt::CaseInsensitive ) == 0
                    || style.compare( "size", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2[ 0 ].isLetter() || cap2.endsWith( "px", Qt::CaseInsensitive )
                 || cap2.endsWith( "pt", Qt::CaseInsensitive ) || cap2.endsWith( "em", Qt::CaseInsensitive )
                 || cap2.endsWith( "%" ) ) {
              newSpan += QString( "font-size:" ) + cap2 + ";";
            }
            else {
              int size = cap2.toInt();
              if ( size ) {
                newSpan += QString( "font-size:%1pt;" ).arg( size / 1024.0, 0, 'f', 3 );
              }
            }
          }
          else if ( style.compare( "font_style", Qt::CaseInsensitive ) == 0
                    || style.compare( "style", Qt::CaseInsensitive ) == 0 ) {
            newSpan += QString( "font-style:" ) + cap2 + ";";
          }
          else if ( style.compare( "font_weight", Qt::CaseInsensitive ) == 0
                    || style.compare( "weight", Qt::CaseInsensitive ) == 0 ) {
            QString str = cap2;
            if ( str.compare( "ultralight", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-weight:100;" );
            }
            else if ( str.compare( "light", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-weight:200;" );
            }
            else if ( str.compare( "ultrabold", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-weight:800;" );
            }
            else if ( str.compare( "heavy", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-weight:900" );
            }
            else {
              newSpan += QString( "font-weight:" ) + str + ";";
            }
          }
          else if ( style.compare( "font_variant", Qt::CaseInsensitive ) == 0
                    || style.compare( "variant", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2.compare( "smallcaps", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-variant:small-caps" );
            }
            else {
              newSpan += QString( "font-variant:" ) + cap2 + ";";
            }
          }
          else if ( style.compare( "font_stretch", Qt::CaseInsensitive ) == 0
                    || style.compare( "stretch", Qt::CaseInsensitive ) == 0 ) {
            QString str = cap2;
            if ( str.compare( "ultracondensed", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:ultra-condensed;" );
            }
            else if ( str.compare( "extracondensed", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:extra-condensed;" );
            }
            else if ( str.compare( "semicondensed", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:semi-condensed;" );
            }
            else if ( str.compare( "semiexpanded", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:semi-expanded;" );
            }
            else if ( str.compare( "extraexpanded", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:extra-expanded;" );
            }
            else if ( str.compare( "ultraexpanded", Qt::CaseInsensitive ) == 0 ) {
              newSpan += QString( "font-stretch:ultra-expanded;" );
            }
            else {
              newSpan += QString( "font-stretch:" ) + str + ";";
            }
          }
          else if ( style.compare( "foreground", Qt::CaseInsensitive ) == 0
                    || style.compare( "fgcolor", Qt::CaseInsensitive ) == 0
                    || style.compare( "color", Qt::CaseInsensitive ) == 0 ) {
            newSpan += QString( "color:" ) + cap2 + ";";
          }
          else if ( style.compare( "background", Qt::CaseInsensitive ) == 0
                    || style.compare( "bgcolor", Qt::CaseInsensitive ) == 0 ) {
            newSpan += QString( "background-color:" ) + cap2 + ";";
          }
          else if ( style.compare( "underline_color", Qt::CaseInsensitive ) == 0
                    || style.compare( "strikethrough_color", Qt::CaseInsensitive ) == 0 ) {
            newSpan += QString( "text-decoration-color:" ) + cap2 + ";";
          }
          else if ( style.compare( "underline", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2.compare( "none", Qt::CaseInsensitive ) ) {
              newSpan += QString( "text-decoration-line:none;" );
            }
            else {
              newSpan += QString( "text-decoration-line:underline; " );
              if ( cap2.compare( "low", Qt::CaseInsensitive ) ) {
                newSpan += QString( "text-decoration-style:dotted;" );
              }
              else if ( cap2.compare( "single", Qt::CaseInsensitive ) ) {
                newSpan += QString( "text-decoration-style:solid;" );
              }
              else if ( cap2.compare( "error", Qt::CaseInsensitive ) ) {
                newSpan += QString( "text-decoration-style:wavy;" );
              }
              else {
                newSpan += QString( "text-decoration-style:" ) + cap2 + ";";
              }
            }
          }
          else if ( style.compare( "strikethrough", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2.compare( "true", Qt::CaseInsensitive ) ) {
              newSpan += QString( "text-decoration-line:line-through;" );
            }
            else {
              newSpan += QString( "text-decoration-line:none;" );
            }
          }
          else if ( style.compare( "rise", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2.endsWith( "px", Qt::CaseInsensitive ) || cap2.endsWith( "pt", Qt::CaseInsensitive )
                 || cap2.endsWith( "em", Qt::CaseInsensitive ) || cap2.endsWith( "%" ) ) {
              newSpan += QString( "vertical-align:" ) + cap2 + ";";
            }
            else {
              int riseValue = cap2.toInt();
              if ( riseValue ) {
                newSpan += QString( "vertical-align:%1pt;" ).arg( riseValue / 1024.0, 0, 'f', 3 );
              }
            }
          }
          else if ( style.compare( "letter_spacing", Qt::CaseInsensitive ) == 0 ) {
            if ( cap2.endsWith( "px", Qt::CaseInsensitive ) || cap2.endsWith( "pt", Qt::CaseInsensitive )
                 || cap2.endsWith( "em", Qt::CaseInsensitive ) || cap2.endsWith( "%" ) ) {
              newSpan += QString( "letter-spacing:" ) + cap2 + ";";
            }
            else {
              int spacing = cap2.toInt();
              if ( spacing ) {
                newSpan += QString( "letter-spacing:%1pt;" ).arg( spacing / 1024.0, 0, 'f', 3 );
              }
            }
          }

          stylePos += styleMatch.capturedLength();
        }
      } while ( stylePos >= 0 );

      newSpan += "\">";
      text.replace( pos, match.capturedLength(), newSpan );
      pos += newSpan.size();
    }
  } while ( pos >= 0 );

  text.replace( "  ", "&nbsp;&nbsp;" );
}

void StardictDictionary::loadArticle( uint32_t address, string & headword, string & articleText )
{
  uint32_t offset, size;

  getArticleProps( address, headword, offset, size );

  char * articleBody;

  {
    QMutexLocker _( &dzMutex );

    // Note that the function always zero-pads the result.
    articleBody = dict_data_read_( dz, offset, size, 0, 0 );
  }

  if ( !articleBody ) {
    //    throw exCantReadFile( getDictionaryFilenames()[ 2 ] );
    articleText = string( "<div class=\"sdict_m\">DICTZIP error: " ) + dict_error_str( dz ) + "</div>";
    return;
  }

  articleText.clear();

  char * ptr = articleBody;

  if ( !sameTypeSequence.empty() ) {
    /// The sequence is known, it's not stored in the article itself
    for ( unsigned seq = 0; seq < sameTypeSequence.size(); ++seq ) {
      // Last entry doesn't have size info -- it is inferred from
      // the bytes left
      bool entrySizeKnown = ( seq == sameTypeSequence.size() - 1 );

      uint32_t entrySize = 0;

      if ( entrySizeKnown ) {
        entrySize = size;
      }
      else if ( !size ) {
        qWarning( "Stardict: short entry for the word %s encountered in \"%s\".", headword.c_str(), getName().c_str() );
        break;
      }

      char type = sameTypeSequence[ seq ];

      if ( islower( type ) ) {
        // Zero-terminated entry, unless it's the last one
        if ( !entrySizeKnown ) {
          entrySize = strlen( ptr );
        }

        if ( size < entrySize ) {
          qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                    headword.c_str(),
                    getName().c_str() );
          break;
        }

        articleText += handleResource( type, ptr, entrySize );

        if ( !entrySizeKnown ) {
          ++entrySize; // Need to skip the zero byte
        }

        ptr += entrySize;
        size -= entrySize;
      }
      else if ( isupper( *ptr ) ) {
        // An entry which has its size before contents, unless it's the last one

        if ( !entrySizeKnown ) {
          if ( size < sizeof( uint32_t ) ) {
            qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                      headword.c_str(),
                      getName().c_str() );
            break;
          }

          memcpy( &entrySize, ptr, sizeof( uint32_t ) );

          entrySize = ntohl( entrySize );

          ptr += sizeof( uint32_t );
          size -= sizeof( uint32_t );
        }

        if ( size < entrySize ) {
          qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                    headword.c_str(),
                    getName().c_str() );
          break;
        }

        articleText += handleResource( type, ptr, entrySize );

        ptr += entrySize;
        size -= entrySize;
      }
      else {
        qWarning( "Stardict: non-alpha entry type 0x%x for the word %s encountered in \"%s\".",
                  type,
                  headword.c_str(),
                  getName().c_str() );
        break;
      }
    }
  }
  else {
    // The sequence is stored in each article separately
    while ( size ) {
      if ( islower( *ptr ) ) {
        // Zero-terminated entry
        size_t len = strlen( ptr + 1 );

        if ( size < len + 2 ) {
          qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                    headword.c_str(),
                    getName().c_str() );
          break;
        }

        articleText += handleResource( *ptr, ptr + 1, len );

        ptr += len + 2;
        size -= len + 2;
      }
      else if ( isupper( *ptr ) ) {
        // An entry which havs its size before contents
        if ( size < sizeof( uint32_t ) + 1 ) {
          qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                    headword.c_str(),
                    getName().c_str() );
          break;
        }

        uint32_t entrySize;

        memcpy( &entrySize, ptr + 1, sizeof( uint32_t ) );

        entrySize = ntohl( entrySize );

        if ( size < sizeof( uint32_t ) + 1 + entrySize ) {
          qWarning( "Stardict: malformed entry for the word %s encountered in \"%s\".",
                    headword.c_str(),
                    getName().c_str() );
          break;
        }

        articleText += handleResource( *ptr, ptr + 1 + sizeof( uint32_t ), entrySize );

        ptr += sizeof( uint32_t ) + 1 + entrySize;
        size -= sizeof( uint32_t ) + 1 + entrySize;
      }
      else {
        qWarning( "Stardict: non-alpha entry type 0x%x for the word %s encountered in \"%s\".",
                  (unsigned)*ptr,
                  headword.c_str(),
                  getName().c_str() );
        break;
      }
    }
  }

  free( articleBody );
}

const QString & StardictDictionary::getDescription()
{
  if ( !dictionaryDescription.isEmpty() ) {
    return dictionaryDescription;
  }

  Ifo ifo( QString::fromStdString( getDictionaryFilenames()[ 0 ] ) );

  if ( !ifo.copyright.empty() ) {
    QString copyright = QString::fromUtf8( ifo.copyright.c_str() );
    dictionaryDescription += QObject::tr( "Copyright: %1%2" ).arg( copyright ).arg( "<br><br>" );
  }

  if ( !ifo.author.empty() ) {
    QString author = QString::fromUtf8( ifo.author.c_str() );
    dictionaryDescription += QObject::tr( "Author: %1%2" ).arg( author ).arg( "<br><br>" );
  }

  if ( !ifo.email.empty() ) {
    QString email = QString::fromUtf8( ifo.email.c_str() );
    dictionaryDescription += QObject::tr( "E-mail: %1%2" ).arg( email ).arg( "<br><br>" );
  }

  if ( !ifo.website.empty() ) {
    QString website = QString::fromUtf8( ifo.website.c_str() );
    dictionaryDescription += QObject::tr( "Website: %1%2" ).arg( website ).arg( "<br><br>" );
  }

  if ( !ifo.date.empty() ) {
    QString date = QString::fromUtf8( ifo.date.c_str() );
    dictionaryDescription += QObject::tr( "Date: %1%2" ).arg( date ).arg( "<br><br>" );
  }

  if ( !ifo.description.empty() ) {
    QString desc = QString::fromUtf8( ifo.description.c_str() );
    dictionaryDescription += desc;
  }

  if ( dictionaryDescription.isEmpty() ) {
    dictionaryDescription = "NONE";
  }

  return dictionaryDescription;
}

QString StardictDictionary::getMainFilename()
{
  return getDictionaryFilenames()[ 0 ].c_str();
}

void StardictDictionary::makeFTSIndex( QAtomicInt & isCancelled )
{
  if ( !( Dictionary::needToRebuildIndex( getDictionaryFilenames(), ftsIdxName )
          || FtsHelpers::ftsIndexIsOldOrBad( this ) ) ) {
    FTS_index_completed.ref();
  }

  if ( haveFTSIndex() ) {
    return;
  }

  if ( ensureInitDone().size() ) {
    return;
  }


  qDebug( "Stardict: Building the full-text index for dictionary: %s", getName().c_str() );

  try {
    FtsHelpers::makeFTSIndex( this, isCancelled );
    FTS_index_completed.ref();
  }
  catch ( std::exception & ex ) {
    qWarning( "Stardict: Failed building full-text search index for \"%s\", reason: %s", getName().c_str(), ex.what() );
    QFile::remove( ftsIdxName.c_str() );
  }
}

void StardictDictionary::getArticleText( uint32_t articleAddress, QString & headword, QString & text )
{
  try {
    string headwordStr, articleStr;
    loadArticle( articleAddress, headwordStr, articleStr );

    headword = QString::fromUtf8( headwordStr.data(), headwordStr.size() );

    text = Html::unescape( QString::fromStdString( articleStr ) );
  }
  catch ( std::exception & ex ) {
    qWarning( "Stardict: Failed retrieving article from \"%s\", reason: %s", getName().c_str(), ex.what() );
  }
}

sptr< Dictionary::DataRequest > StardictDictionary::getSearchResults( const QString & searchString,
                                                                      int searchMode,
                                                                      bool matchCase,
                                                                      bool ignoreDiacritics )
{
  return std::make_shared< FtsHelpers::FTSResultsRequest >( *this,
                                                            searchString,
                                                            searchMode,
                                                            matchCase,
                                                            ignoreDiacritics );
}

/// StardictDictionary::findHeadwordsForSynonym()

class StardictHeadwordsRequest: public Dictionary::WordSearchRequest
{

  std::u32string word;
  StardictDictionary & dict;

  QAtomicInt isCancelled;
  QFuture< void > f;

public:

  StardictHeadwordsRequest( const std::u32string & word_, StardictDictionary & dict_ ):
    word( word_ ),
    dict( dict_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~StardictHeadwordsRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};


void StardictHeadwordsRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  try {
    //limited the synomys to at most 10 entries
    vector< WordArticleLink > chain = dict.findArticles( word, false, 10 );

    std::u32string caseFolded = Folding::applySimpleCaseOnly( word );

    for ( auto & x : chain ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        finish();
        return;
      }

      string headword, articleText;

      dict.loadArticle( x.articleOffset, headword, articleText );

      std::u32string headwordDecoded = Text::toUtf32( headword );

      if ( caseFolded != Folding::applySimpleCaseOnly( headwordDecoded ) ) {
        // The headword seems to differ from the input word, which makes the
        // input word its synonym.
        QMutexLocker _( &dataMutex );

        matches.push_back( headwordDecoded );
      }
    }
  }
  catch ( std::exception & e ) {
    setErrorString( QString::fromUtf8( e.what() ) );
  }

  finish();
}

sptr< Dictionary::WordSearchRequest > StardictDictionary::findHeadwordsForSynonym( const std::u32string & word )
{
  return synonymSearchEnabled ? std::make_shared< StardictHeadwordsRequest >( word, *this ) :
                                Class::findHeadwordsForSynonym( word );
}


/// StardictDictionary::getArticle()


class StardictArticleRequest: public Dictionary::DataRequest
{

  std::u32string word;
  vector< std::u32string > alts;
  StardictDictionary & dict;
  bool ignoreDiacritics;

  QAtomicInt isCancelled;
  QFuture< void > f;


public:

  StardictArticleRequest( const std::u32string & word_,
                          const vector< std::u32string > & alts_,
                          StardictDictionary & dict_,
                          bool ignoreDiacritics_ ):
    word( word_ ),
    alts( alts_ ),
    dict( dict_ ),
    ignoreDiacritics( ignoreDiacritics_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();

  void cancel() override
  {
    isCancelled.ref();
  }

  ~StardictArticleRequest()
  {
    isCancelled.ref();
    f.waitForFinished();
  }
};

void StardictArticleRequest::run()
{
  if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
    finish();
    return;
  }

  try {
    vector< WordArticleLink > chain = dict.findArticles( word, ignoreDiacritics );

    //if alts has more than 100 , great probability that the dictionary is wrong produced or parsed.
    if ( alts.size() < 100 ) {
      for ( const auto & alt : alts ) {
        /// Make an additional query for each alt

        vector< WordArticleLink > altChain = dict.findArticles( alt, ignoreDiacritics );
        if ( altChain.size() > 100 ) {
          continue;
        }
        chain.insert( chain.end(), altChain.begin(), altChain.end() );
      }
    }

    multimap< std::u32string, pair< string, string > > mainArticles, alternateArticles;

    set< uint32_t > articlesIncluded; // Some synonyms make it that the articles
                                      // appear several times. We combat this
                                      // by only allowing them to appear once.

    std::u32string wordCaseFolded = Folding::applySimpleCaseOnly( word );
    if ( ignoreDiacritics ) {
      wordCaseFolded = Folding::applyDiacriticsOnly( wordCaseFolded );
    }

    //if the chain is too large, it is more likely has some dictionary making or parsing issue.
    for ( unsigned x = 0; x < qMin( 10U, (unsigned)chain.size() ); ++x ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        finish();
        return;
      }

      if ( articlesIncluded.find( chain[ x ].articleOffset ) != articlesIncluded.end() ) {
        continue; // We already have this article in the body.
      }

      // Now grab that article

      string headword, articleText;

      dict.loadArticle( chain[ x ].articleOffset, headword, articleText );

      // Ok. Now, does it go to main articles, or to alternate ones? We list
      // main ones first, and alternates after.

      // We do the case-folded comparison here.

      std::u32string headwordStripped = Folding::applySimpleCaseOnly( headword );
      if ( ignoreDiacritics ) {
        headwordStripped = Folding::applyDiacriticsOnly( headwordStripped );
      }

      multimap< std::u32string, pair< string, string > > & mapToUse =
        ( wordCaseFolded == headwordStripped ) ? mainArticles : alternateArticles;

      mapToUse.insert( pair( Folding::applySimpleCaseOnly( headword ), pair( headword, articleText ) ) );

      articlesIncluded.insert( chain[ x ].articleOffset );
    }

    if ( mainArticles.empty() && alternateArticles.empty() ) {
      // No such word
      finish();
      return;
    }

    string result;

    multimap< std::u32string, pair< string, string > >::const_iterator i;

    string cleaner = Utils::Html::getHtmlCleaner();

    for ( i = mainArticles.begin(); i != mainArticles.end(); ++i ) {
      result += dict.isFromLanguageRTL() ? R"(<h3 class="sdct_headwords" dir="rtl">)" : "<h3 class=\"sdct_headwords\">";
      result += i->second.first;
      result += "</h3>";
      if ( dict.isToLanguageRTL() ) {
        result += R"(<div style="display:inline;" dir="rtl">)";
      }
      result += i->second.second;
      result += cleaner;
      if ( dict.isToLanguageRTL() ) {
        result += "</div>";
      }
    }

    for ( i = alternateArticles.begin(); i != alternateArticles.end(); ++i ) {
      result += dict.isFromLanguageRTL() ? R"(<h3 class="sdct_headwords" dir="rtl">)" : "<h3 class=\"sdct_headwords\">";
      result += i->second.first;
      result += "</h3>";
      if ( dict.isToLanguageRTL() ) {
        result += R"(<div style="display:inline;" dir="rtl">)";
      }
      result += i->second.second;
      result += cleaner;
      if ( dict.isToLanguageRTL() ) {
        result += "</div>";
      }
    }

    appendString( result );

    hasAnyData = true;
  }
  catch ( std::exception & e ) {
    setErrorString( QString::fromUtf8( e.what() ) );
  }

  finish();
}

sptr< Dictionary::DataRequest > StardictDictionary::getArticle( const std::u32string & word,
                                                                const vector< std::u32string > & alts,
                                                                const std::u32string &,
                                                                bool ignoreDiacritics )

{
  return std::make_shared< StardictArticleRequest >( word, alts, *this, ignoreDiacritics );
}


/**
 * @brief Checks if the string 'str' begins with the substring 'substr'.
 *
 * Compares the characters of 'substr' with the beginning of 'str'. If all characters
 * in 'substr' match the corresponding characters in 'str', returns a pointer to the
 * position in 'str' immediately after the matched substring. If 'substr' is not a
 * prefix of 'str', or if either pointer is null, returns nullptr.
 *
 * @param substr The substring to check as a prefix.
 * @param str The string to check against.
 * @return const char* Pointer to the character in 'str' after the matched prefix,
 *         or nullptr if 'substr' is not a prefix of 'str' or if any argument is null.
 */
static const char * beginsWith( const char * substr, const char * str )
{
  if ( !substr || !str )
    return nullptr;
  for ( ; *substr; ++substr, ++str )
    if ( *substr != *str )
      return nullptr;
  return str;
}


Ifo::Ifo( const QString & fileName )
{
  QFile f( fileName );
  if ( !f.open( QIODevice::ReadOnly ) ) {
    throw exCantReadFile( "Cannot open IFO file -> " + fileName.toStdString() );
  };

  // Set up text stream with automatic Unicode detection
  QTextStream stream( &f );
  stream.setAutoDetectUnicode( true );

  // Read and validate file format
  QString firstLine  = stream.readLine();
  QString secondLine = stream.readLine();

  // Ensure proper file format detection even with BOM
  if ( !firstLine.startsWith( "StarDict's dict ifo file" ) || !secondLine.startsWith( "version=" ) ) {
    throw exNotAnIfoFile();
  }

  /// Now go through the file and parse options
  {
    while ( !stream.atEnd() ) {
      QString line       = stream.readLine();
      QStringView option = QStringView( line ).trimmed();
      // Empty lines are allowed in .ifo file

      if ( option.isEmpty() ) {
        continue;
      }

      // Convert QStringView to UTF-8 encoded const char*
      QByteArray optionUtf8   = option.toString().toUtf8();
      const char * optionData = optionUtf8.constData();

      if ( const char * val = beginsWith( "bookname=", optionData ) ) {
        bookname = val;
      }
      else if ( const char * val = beginsWith( "wordcount=", optionData ) ) {
        auto [ ptr, ec ] = std::from_chars( val, val + strlen( val ), wordcount );
        if ( ec != std::errc{} ) {
          throw exBadFieldInIfo( optionData );
        }
      }
      else if ( const char * val = beginsWith( "synwordcount=", optionData ) ) {
        auto [ ptr, ec ] = std::from_chars( val, val + strlen( val ), synwordcount );
        if ( ec != std::errc{} ) {
          throw exBadFieldInIfo( optionData );
        }
      }
      else if ( const char * val = beginsWith( "idxfilesize=", optionData ) ) {
        auto [ ptr, ec ] = std::from_chars( val, val + strlen( val ), idxfilesize );
        if ( ec != std::errc{} ) {
          throw exBadFieldInIfo( optionData );
        }
      }
      else if ( const char * val = beginsWith( "idxoffsetbits=", optionData ) ) {
        auto [ ptr, ec ] = std::from_chars( val, val + strlen( val ), idxoffsetbits );
        if ( ec != std::errc{} || ( idxoffsetbits != 32 && idxoffsetbits != 64 ) ) {
          throw exBadFieldInIfo( optionData );
        }
      }
      else if ( const char * val = beginsWith( "sametypesequence=", optionData ) ) {
        sametypesequence = val;
      }
      else if ( const char * val = beginsWith( "dicttype=", optionData ) ) {
        dicttype = val;
      }
      else if ( const char * val = beginsWith( "description=", optionData ) ) {
        description = val;
      }
      else if ( const char * val = beginsWith( "copyright=", optionData ) ) {
        copyright = val;
      }
      else if ( const char * val = beginsWith( "author=", optionData ) ) {
        author = val;
      }
      else if ( const char * val = beginsWith( "email=", optionData ) ) {
        email = val;
      }
      else if ( const char * val = beginsWith( "website=", optionData ) ) {
        website = val;
      }
      else if ( const char * val = beginsWith( "date=", optionData ) ) {
        date = val;
      }
    }
  }
}

//// StardictDictionary::getResource()


class StardictResourceRequest: public ResourceRequest
{

  StardictDictionary & dict;

  string resourceName;


public:

  StardictResourceRequest( StardictDictionary & dict_, const string & resourceName_ ):
    dict( dict_ ),
    resourceName( resourceName_ )
  {
    f = QtConcurrent::run( [ this ]() {
      this->run();
    } );
  }

  void run();
};

void StardictResourceRequest::run()
{


  try {
    if ( resourceName.at( 0 ) == '\x1E' ) {
      resourceName = resourceName.erase( 0, 1 );
    }
    if ( resourceName.at( resourceName.length() - 1 ) == '\x1F' ) {
      resourceName.erase( resourceName.length() - 1, 1 );
    }

    qDebug() << "startdict resource name is" << resourceName.c_str();

    {
      QMutexLocker _( &dataMutex );
      auto resFile =
        QFile( dict.getContainingFolder() + QStringLiteral( "/res/" ) + QString::fromStdString( resourceName ) );
      if ( resFile.open( QIODevice::ReadOnly ) ) {
        data.resize( resFile.size() );
        resFile.read( data.data(), data.size() );
      }
      else {
        if ( !( dict.resourceZip.isOpen() && dict.resourceZip.loadFile( Text::toUtf32( resourceName ), data ) ) ) {
          throw std::runtime_error( "Cannot read from either res folder or res.zip" );
        }
      }
    }


    if ( Filetype::isNameOfTiff( resourceName ) ) {
      // Convert it

      QMutexLocker _( &dataMutex );
      GdTiff::tiff2img( data );
    }

    if ( Filetype::isNameOfCSS( resourceName ) ) {
      QMutexLocker _( &dataMutex );

      QString css = QString::fromUtf8( data.data(), data.size() );

      // Correct some url's

      QString id = QString::fromUtf8( dict.getId().c_str() );
      int pos    = 0;

      static QRegularExpression links( R"(url\(\s*(['"]?)([^'"]*)(['"]?)\s*\))",
                                       QRegularExpression::CaseInsensitiveOption );

      QString newCSS;
      QRegularExpressionMatchIterator it = links.globalMatch( css );
      while ( it.hasNext() ) {
        QRegularExpressionMatch match = it.next();
        newCSS += css.mid( pos, match.capturedStart() - pos );
        pos = match.capturedEnd();

        QString url = match.captured( 2 );

        if ( url.indexOf( ":/" ) >= 0 || url.indexOf( "data:" ) >= 0 ) {
          // External link
          newCSS += match.captured();
          continue;
        }

        QString newUrl =
          QString( "url(" ) + match.captured( 1 ) + "bres://" + id + "/" + url + match.captured( 3 ) + ")";
        newCSS += newUrl;
      }
      if ( pos ) {
        newCSS += css.mid( pos );
        css = newCSS;
        newCSS.clear();
      }

      dict.isolateCSS( css );
      QByteArray bytes = css.toUtf8();
      data.resize( bytes.size() );
      memcpy( &data.front(), bytes.constData(), bytes.size() );
    }

    QMutexLocker _( &dataMutex );
    hasAnyData = true;
  }
  catch ( std::exception & ex ) {
    qWarning( "Stardict: Failed loading resource \"%s\" for \"%s\", reason: %s",
              resourceName.c_str(),
              dict.getName().c_str(),
              ex.what() );
    // Resource not loaded -- we don't set the hasAnyData flag then
  }
  catch ( ... ) {
  }

  finish();
}

sptr< ResourceRequest > StardictDictionary::getResource( const string & name )

{
  return std::make_shared< StardictResourceRequest >( *this, name );
}

} // anonymous namespace

static void findCorrespondingFiles( const string & ifo, string & idx, string & dict, string & syn )
{
  string base( ifo, 0, ifo.size() - 3 );

  idx = Utils::Fs::findFirstExistingFile(
    { base + "idx", base + "idx.gz", base + "idx.dz", base + "IDX", base + "IDX.GZ", base + "IDX.DZ" } );
  if ( idx.empty() ) {
    throw exNoIdxFile( ifo );
  }

  dict = Utils::Fs::findFirstExistingFile( { base + "dict", base + "dict.dz", base + "DICT", base + "dict.DZ" } );
  if ( dict.empty() ) {
    throw exNoDictFile( ifo );
  }

  syn = Utils::Fs::findFirstExistingFile(
    { base + "syn", base + "syn.gz", base + "syn.dz", base + "SYN", base + "SYN.GZ", base + "SYN.DZ" } );

  if ( syn.empty() ) {
    syn.clear();
  }
}

static void handleIdxSynFile( const string & fileName,
                              IndexedWords & indexedWords,
                              ChunkedStorage::Writer & chunks,
                              vector< uint32_t > * articleOffsets,
                              bool isSynFile,
                              bool parseHeadwords )
{
  gzFile stardictIdx = gd_gzopen( fileName.c_str() );
  if ( !stardictIdx ) {
    throw exCantReadFile( fileName );
  }

  vector< char > image;

  for ( ;; ) {
    size_t oldSize = image.size();

    image.resize( oldSize + 65536 );

    int rd = gzread( stardictIdx, &image.front() + oldSize, 65536 );

    if ( rd < 0 ) {
      gzclose( stardictIdx );
      throw exCantReadFile( fileName );
    }

    if ( rd != 65536 ) {
      image.resize( oldSize + rd + 1 );
      break;
    }
  }
  gzclose( stardictIdx );

  // We append one zero byte to catch runaway string at the end, if any

  image.back() = 0;

  // Now parse it

  for ( const char * ptr = &image.front(); ptr != &image.back(); ) {
    size_t wordLen = strlen( ptr );

    if ( ptr + wordLen + 1 + ( isSynFile ? sizeof( uint32_t ) : sizeof( uint32_t ) * 2 ) > &image.back() ) {
      qWarning( "Warning: sudden end of file %s", fileName.c_str() );
      break;
    }

    const char * word = ptr;

    ptr += wordLen + 1;

    uint32_t offset;

    if ( strstr( word, "&#" ) ) {
      // Decode some html-coded symbols in headword
      string unescapedWord = Html::unescapeUtf8( word );
      strncpy( (char *)word, unescapedWord.c_str(), wordLen );
      wordLen = strlen( word );
    }

    if ( !isSynFile ) {
      // We're processing the .idx file
      uint32_t articleOffset, articleSize;

      memcpy( &articleOffset, ptr, sizeof( uint32_t ) );
      ptr += sizeof( uint32_t );
      memcpy( &articleSize, ptr, sizeof( uint32_t ) );
      ptr += sizeof( uint32_t );

      articleOffset = ntohl( articleOffset );
      articleSize   = ntohl( articleSize );

      // Create an entry for the article in the chunked storage

      offset = chunks.startNewBlock();

      if ( articleOffsets ) {
        articleOffsets->push_back( offset );
      }

      chunks.addToBlock( &articleOffset, sizeof( uint32_t ) );
      chunks.addToBlock( &articleSize, sizeof( uint32_t ) );
      chunks.addToBlock( word, wordLen + 1 );
    }
    else {
      // We're processing the .syn file
      uint32_t offsetInIndex;

      memcpy( &offsetInIndex, ptr, sizeof( uint32_t ) );
      ptr += sizeof( uint32_t );

      offsetInIndex = ntohl( offsetInIndex );

      if ( offsetInIndex >= articleOffsets->size() ) {
        throw exIncorrectOffset( fileName );
      }

      offset = ( *articleOffsets )[ offsetInIndex ];

      // Some StarDict dictionaries are in fact badly converted Babylon ones.
      // They contain a lot of superfluous slashed entries with dollar signs.
      // We try to filter them out here, since those entries become much more
      // apparent in GoldenDict than they were in StarDict because of
      // punctuation folding. Hopefully there are not a whole lot of valid
      // synonyms which really start from slash and contain dollar signs, or
      // end with dollar and contain slashes.
      if ( *word == '/' ) {
        if ( strchr( word, '$' ) ) {
          continue; // Skip this entry
        }
      }
      else if ( wordLen && word[ wordLen - 1 ] == '$' ) {
        if ( strchr( word, '/' ) ) {
          continue; // Skip this entry
        }
      }

      // if the entry is hypen, skip
      if ( wordLen == 1 && *word == '-' ) {
        continue; // Skip this entry
      }
    }

    // Insert new entry into an index

    if ( parseHeadwords ) {
      indexedWords.addWord( Text::toUtf32( word ), offset );
    }
    else {
      indexedWords.addSingleWord( Text::toUtf32( word ), offset );
    }
  }

  qDebug( "%u entires made", (unsigned)indexedWords.size() );
}


vector< sptr< Dictionary::Class > > makeDictionaries( const vector< string > & fileNames,
                                                      const string & indicesDir,
                                                      Dictionary::Initializing & initializing,
                                                      unsigned maxHeadwordsToExpand )

{
  vector< sptr< Dictionary::Class > > dictionaries;

  for ( const auto & fileName : fileNames ) {
    if ( !Utils::endsWithIgnoreCase( fileName, ".ifo" ) ) {
      continue;
    }

    try {
      vector< string > dictFiles( 1, fileName );

      string idxFileName, dictFileName, synFileName;

      findCorrespondingFiles( fileName, idxFileName, dictFileName, synFileName );

      dictFiles.push_back( idxFileName );
      dictFiles.push_back( dictFileName );

      if ( synFileName.size() ) {
        dictFiles.push_back( synFileName );
      }

      // See if there's a res.zip file
      string zipFileName;
      {
        auto info = QFileInfo( QString::fromStdString( idxFileName ) );
        for ( auto & i :
              { QStringLiteral( "res.zip" ), info.baseName() + ".res.zip", QStringLiteral( "res/res.zip" ) } ) {
          if ( File::tryPossibleZipName( info.absoluteDir().absoluteFilePath( i ).toStdString(), zipFileName ) ) {
            dictFiles.push_back( zipFileName );
          };
        }
      }

      string dictId = Dictionary::makeDictionaryId( dictFiles );

      string indexFile = indicesDir + dictId;

      if ( Dictionary::needToRebuildIndex( dictFiles, indexFile ) || indexIsOldOrBad( indexFile ) ) {
        // Building the index

        Ifo ifo( QString::fromStdString( fileName ) );

        qDebug( "Stardict: Building the index for dictionary: %s", ifo.bookname.c_str() );

        if ( ifo.idxoffsetbits == 64 ) {
          throw ex64BitsNotSupported();
        }

        if ( ifo.dicttype.size() ) {
          throw exDicttypeNotSupported();
        }

        if ( synFileName.empty() ) {
          if ( ifo.synwordcount ) {
            qDebug(
              "Warning: dictionary has synwordcount specified, but no "
              "corresponding .syn file was found\n" );
            ifo.synwordcount = 0; // Pretend it wasn't there
          }
        }
        else if ( !ifo.synwordcount ) {
          qDebug( "Warning: ignoring .syn file %s, since there's no synwordcount in .ifo specified",
                  synFileName.c_str() );
        }


        qDebug( "bookname = %s", ifo.bookname.c_str() );
        qDebug( "wordcount = %u", ifo.wordcount );

        initializing.indexingDictionary( ifo.bookname );

        File::Index idx( indexFile, QIODevice::WriteOnly );

        IdxHeader idxHeader;

        memset( &idxHeader, 0, sizeof( idxHeader ) );

        // We write a dummy header first. At the end of the process the header
        // will be rewritten with the right values.

        idx.write( idxHeader );

        idx.write( ifo.bookname.data(), ifo.bookname.size() );
        idx.write( ifo.sametypesequence.data(), ifo.sametypesequence.size() );

        IndexedWords indexedWords;

        ChunkedStorage::Writer chunks( idx );

        // Load indices
        if ( !ifo.synwordcount ) {
          handleIdxSynFile( idxFileName,
                            indexedWords,
                            chunks,
                            0,
                            false,
                            !maxHeadwordsToExpand || ifo.wordcount < maxHeadwordsToExpand );
        }
        else {
          vector< uint32_t > articleOffsets;

          articleOffsets.reserve( ifo.wordcount );

          handleIdxSynFile( idxFileName,
                            indexedWords,
                            chunks,
                            &articleOffsets,
                            false,
                            !maxHeadwordsToExpand || ( ifo.wordcount + ifo.synwordcount ) < maxHeadwordsToExpand );

          handleIdxSynFile( synFileName,
                            indexedWords,
                            chunks,
                            &articleOffsets,
                            true,
                            !maxHeadwordsToExpand || ( ifo.wordcount + ifo.synwordcount ) < maxHeadwordsToExpand );
        }

        // Finish with the chunks

        idxHeader.chunksOffset = chunks.finish();

        // Build index

        IndexInfo idxInfo = BtreeIndexing::buildIndex( indexedWords, idx );

        idxHeader.indexBtreeMaxElements = idxInfo.btreeMaxElements;
        idxHeader.indexRootOffset       = idxInfo.rootOffset;

        // That concludes it. Update the header.

        idxHeader.signature     = Signature;
        idxHeader.formatVersion = CurrentFormatVersion;

        idxHeader.wordCount            = ifo.wordcount;
        idxHeader.synWordCount         = ifo.synwordcount;
        idxHeader.bookNameSize         = ifo.bookname.size();
        idxHeader.sameTypeSequenceSize = ifo.sametypesequence.size();

        // read languages from dictioanry file name
        auto langs = LangCoder::findLangIdPairFromName( QString::fromStdString( dictFileName ) );
        // if no languages found, try dictionary's name
        if ( langs.first == 0 || langs.second == 0 ) {
          langs = LangCoder::findLangIdPairFromName( QString::fromStdString( ifo.bookname ) );
        }

        idxHeader.langFrom = langs.first;
        idxHeader.langTo   = langs.second;

        // If there was a zip file, index it too

        if ( !zipFileName.empty() ) {
          qDebug( "Indexing zip file" );

          idxHeader.hasZipFile = 1;

          IndexedWords zipFileNames;
          IndexedZip zipFile;
          if ( zipFile.openZipFile( QDir::fromNativeSeparators( zipFileName.c_str() ) ) ) {
            zipFile.indexFile( zipFileNames );
          }

          if ( !zipFileNames.empty() ) {
            // Build the resulting zip file index

            IndexInfo idxInfo = BtreeIndexing::buildIndex( zipFileNames, idx );

            idxHeader.zipIndexBtreeMaxElements = idxInfo.btreeMaxElements;
            idxHeader.zipIndexRootOffset       = idxInfo.rootOffset;
          }
          else {
            // Bad zip file -- no index (though the mark that we have one
            // remains)
            idxHeader.zipIndexBtreeMaxElements = 0;
            idxHeader.zipIndexRootOffset       = 0;
          }
        }
        else {
          idxHeader.hasZipFile = 0;
        }

        // That concludes it. Update the header.

        idx.rewind();

        idx.write( &idxHeader, sizeof( idxHeader ) );
      }

      dictionaries.push_back( std::make_shared< StardictDictionary >( dictId, indexFile, dictFiles ) );
    }
    catch ( std::exception & e ) {
      qWarning( "Stardict dictionary initializing failed: %s, error: %s", fileName.c_str(), e.what() );
    }
  }

  return dictionaries;
}


} // namespace Stardict
