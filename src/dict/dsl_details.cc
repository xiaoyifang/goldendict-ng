/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "dsl_details.hh"

#include "folding.hh"
#include "langcoder.hh"
#include "ufile.hh"
#include "text.hh"

#include <exception>
#include <stdio.h>
#include <wctype.h>
#include <algorithm>

namespace Dsl {
namespace Details {

using std::list;
using Text::Encoding;

static QMap< int, string > lang_codes = {
  { 1, "en" },    { 1033, "en" }, { 2, "ru" },    { 1049, "ru" }, { 1068, "az" }, { 1025, "ar" }, { 1067, "am" },
  { 15, "af" },   { 1078, "af" }, { 9, "eu" },    { 1069, "eu" }, { 1133, "ba" }, { 21, "be" },   { 1059, "be" },
  { 22, "bg" },   { 1026, "bg" }, { 19, "hu" },   { 1038, "hu" }, { 10, "nl" },   { 1043, "nl" }, { 1032, "el" },
  { 1079, "ka" }, { 13, "da" },   { 1030, "da" }, { 16, "id" },   { 1057, "id" }, { 1039, "is" }, { 6, "es" },
  { 7, "es" },    { 3082, "es" }, { 1034, "es" }, { 5, "it" },    { 1040, "it" }, { 1087, "kk" }, { 1595, "ky" },
  { 28, "ch" },   { 29, "ch" },   { 1028, "ch" }, { 2052, "ch" }, { 30, "la" },   { 1540, "la" }, { 1142, "la" },
  { 1062, "lv" }, { 1063, "lt" }, { 1086, "ms" }, { 3, "de" },    { 26, "de" },   { 1031, "de" }, { 32775, "de" },
  { 14, "nb" },   { 1044, "nb" }, { 25, "nn" },   { 2068, "nn" }, { 20, "pl" },   { 1045, "pl" }, { 8, "pt" },
  { 2070, "pt" }, { 1048, "ro" }, { 23, "sr" },   { 3098, "sr" }, { 1051, "sk" }, { 1060, "sl" }, { 17, "sw" },
  { 1089, "sw" }, { 1064, "tg" }, { 1092, "tt" }, { 27, "tr" },   { 1055, "tr" }, { 1090, "tk" }, { 1091, "tz" },
  { 24, "uk" },   { 1058, "uk" }, { 11, "fi" },   { 1035, "fi" }, { 4, "fr" },    { 1036, "fr" }, { 18, "cs" },
  { 1029, "cs" }, { 12, "sv" },   { 1053, "sv" }, { 1061, "et" } };

string findCodeForDslId( int id )
{
  return lang_codes[ id ];
}

bool isAtSignFirst( const std::u32string & str )
{
  // Test if '@' is first in string except spaces and dsl tags
  QRegularExpression reg( R"([ \t]*(?:\[[^\]]+\][ \t]*)*@)", QRegularExpression::PatternOption::CaseInsensitiveOption );
  return QString::fromStdU32String( str ).indexOf( reg ) == 0;
}

/////////////// ArticleDom

std::u32string ArticleDom::Node::renderAsText( bool stripTrsTag ) const
{
  if ( !isTag ) {
    return text;
  }

  std::u32string result;

  for ( const auto & i : *this ) {
    if ( !stripTrsTag || i.tagName != U"!trs" ) {
      result += i.renderAsText( stripTrsTag );
    }
  }

  return result;
}

namespace {

/// @return true if @p tagName equals "mN" where N is a digit
bool is_mN( const std::u32string & tagName )
{
  return tagName.size() == 2 && tagName[ 0 ] == U'm' && iswdigit( tagName[ 1 ] );
}

bool isAnyM( const std::u32string & tagName )
{
  return tagName == U"m" || is_mN( tagName );
}

bool checkM( const std::u32string & dest, const std::u32string & src )
{
  return src == U"m" && is_mN( dest );
}

/// Closing the [mN] tags is optional. Quote from https://documentation.help/ABBYY-Lingvo8/paragraph_form.htm:
/// Any paragraph from this tag until the end of card or until system meets an «[/m]» (margin shift toggle off) tag
struct MustTagBeClosed
{
  bool operator()( const ArticleDom::Node * tag ) const
  {
    Q_ASSERT( tag->isTag );
    return !isAnyM( tag->tagName );
  }
};

} // unnamed namespace

ArticleDom::ArticleDom( const std::u32string & str, const string & dictName, const std::u32string & headword_ ):
  root( Node::Tag(), std::u32string(), std::u32string() ),
  stringPos( str.c_str() ),
  lineStartPos( str.c_str() ),
  transcriptionCount( 0 ),
  mediaCount( 0 ),
  dictionaryName( dictName ),
  headword( headword_ )
{
  list< Node * > stack; // Currently opened tags

  Node * textNode = 0; // A leaf node which currently accumulates text.

  try {
    for ( ;; ) {
      nextChar();

      if ( ch == L'@' && !escaped ) {
        if ( !atSignFirstInLine() ) {
          // Not insided card
          if ( dictName.empty() ) {
            qWarning( "Unescaped '@' symbol found" );
          }
          else {
            qWarning( "Unescaped '@' symbol found in \"%s\"", dictName.c_str() );
          }
        }
        else {
          // Insided card
          std::u32string linkTo;
          nextChar();
          for ( ;; nextChar() ) {
            if ( ch == L'\n' ) {
              break;
            }
            if ( ch != L'\r' ) {
              if ( escaped && ( ch == L'(' || ch == ')' ) ) {
                linkTo.push_back( L'\\' );
              }
              linkTo.push_back( ch );
            }
          }
          linkTo = Folding::trimWhitespace( linkTo );

          if ( !linkTo.empty() ) {
            list< std::u32string > allLinkEntries;
            processUnsortedParts( linkTo, true );
            expandOptionalParts( linkTo, &allLinkEntries );

            for ( auto entry = allLinkEntries.begin(); entry != allLinkEntries.end(); ) {
              if ( !textNode ) {
                Node text = Node( Node::Text(), std::u32string() );

                if ( stack.empty() ) {
                  root.push_back( text );
                  stack.push_back( &root.back() );
                }
                else {
                  stack.back()->push_back( text );
                  stack.push_back( &stack.back()->back() );
                }

                textNode = stack.back();
              }
              textNode->text.push_back( L'-' );
              textNode->text.push_back( L' ' );

              // Close the currently opened text node
              stack.pop_back();
              textNode = 0;

              std::u32string linkText = Folding::trimWhitespace( *entry );
              ArticleDom nodeDom( linkText, dictName, headword_ );

              Node link( Node::Tag(), U"@", std::u32string() );
              for ( auto & n : nodeDom.root ) {
                link.push_back( n );
              }

              ++entry;

              if ( stack.empty() ) {
                root.push_back( link );
                if ( entry != allLinkEntries.end() ) { // Add line break before next entry
                  root.push_back( Node( Node::Tag(), U"br", std::u32string() ) );
                }
              }
              else {
                stack.back()->push_back( link );
                if ( entry != allLinkEntries.end() ) {
                  stack.back()->push_back( Node( Node::Tag(), U"br", std::u32string() ) );
                }
              }
            }

            // Skip to next '@'

            while ( !( ch == L'@' && !escaped && atSignFirstInLine() ) ) {
              nextChar();
            }

            stringPos--;
            ch      = L'\n';
            escaped = false;
          }
        }
      } // if ( ch == L'@' )

      if ( ch == L'[' && !escaped ) {
        // Beginning of a tag.
        bool isClosing;
        std::u32string name;
        std::u32string attrs;

        try {
          do {
            nextChar();
          } while ( Folding::isWhitespace( ch ) );

          if ( ch == L'/' && !escaped ) {
            // A closing tag.
            isClosing = true;
            nextChar();
          }
          else {
            isClosing = false;
          }

          // Read tag's name

          while ( ( ch != L']' || escaped ) && !Folding::isWhitespace( ch ) ) {
            name.push_back( ch );
            nextChar();
          }

          while ( Folding::isWhitespace( ch ) ) {
            nextChar();
          }

          // Read attrs

          while ( ch != L']' || escaped ) {
            attrs.push_back( ch );
            nextChar();
          }
        }
        catch ( std::exception & ex ) {
          if ( !dictionaryName.empty() ) {
            qWarning( R"(DSL: Unfinished tag "%s" with attributes "%s" found in "%s", article "%s".)",
                      QString::fromStdU32String( name ).toUtf8().data(),
                      QString::fromStdU32String( attrs ).toUtf8().data(),
                      dictionaryName.c_str(),
                      QString::fromStdU32String( headword ).toUtf8().data() );
          }
          else {
            qWarning( R"(DSL: Unfinished tag "%s" with attributes "%s" found)",
                      QString::fromStdU32String( name ).toUtf8().data(),
                      QString::fromStdU32String( attrs ).toUtf8().data() );
          }

          throw ex;
        }

        // Add the tag, or close it

        if ( textNode ) {
          // Close the currently opened text node
          stack.pop_back();
          textNode = 0;
        }

        // If the tag is [t], we update the transcriptionCount
        if ( name == U"t" ) {
          if ( isClosing ) {
            if ( transcriptionCount ) {
              --transcriptionCount;
            }
          }
          else {
            ++transcriptionCount;
          }
        }

        // If the tag is [s], we update the mediaCount
        if ( name == U"s" ) {
          if ( isClosing ) {
            if ( mediaCount ) {
              --mediaCount;
            }
          }
          else {
            ++mediaCount;
          }
        }

        if ( !isClosing ) {
          if ( isAnyM( name ) ) {
            // Opening an 'mX' or 'm' tag closes any previous 'm' tag
            closeTag( U"m", stack, false );
          }
          openTag( name, attrs, stack );
          if ( name == U"br" ) {
            // [br] tag don't have closing tag
            closeTag( name, stack );
          }
        }
        else {
          closeTag( name, stack );
        } // if ( isClosing )
        continue;
      } // if ( ch == '[' )

      if ( ch == L'<' && !escaped ) {
        // Special case: the <<name>> link

        nextChar();

        if ( ch != L'<' || escaped ) {
          // Ok, it's not it.
          --stringPos;

          if ( escaped ) {
            --stringPos;
            escaped = false;
          }
          ch = L'<';
        }
        else {
          // Get the link's body
          do {
            nextChar();
          } while ( Folding::isWhitespace( ch ) );

          std::u32string linkTo, linkText;

          for ( ;; nextChar() ) {
            // Is it the end?
            if ( ch == L'>' && !escaped ) {
              nextChar();

              if ( ch == L'>' && !escaped ) {
                break;
              }
              else {
                linkTo.push_back( L'>' );
                linkTo.push_back( ch );

                linkText.push_back( L'>' );
                if ( escaped ) {
                  linkText.push_back( L'\\' );
                }
                linkText.push_back( ch );
              }
            }
            else {
              linkTo.push_back( ch );

              if ( escaped ) {
                linkText.push_back( L'\\' );
              }
              linkText.push_back( ch );
            }
          }

          // Add the corresponding node

          if ( textNode ) {
            // Close the currently opened text node
            stack.pop_back();
            textNode = 0;
          }

          linkText = Folding::trimWhitespace( linkText );
          processUnsortedParts( linkText, true );
          ArticleDom nodeDom( linkText, dictName, headword_ );

          Node link( Node::Tag(), U"ref", std::u32string() );
          for ( auto & n : nodeDom.root ) {
            link.push_back( n );
          }

          if ( stack.empty() ) {
            root.push_back( link );
          }
          else {
            stack.back()->push_back( link );
          }

          continue;
        }
      } // if ( ch == '<' )

      if ( ch == L'{' && !escaped ) {
        // Special case: {{comment}}

        nextChar();

        if ( ch != L'{' || escaped ) {
          // Ok, it's not it.
          --stringPos;

          if ( escaped ) {
            --stringPos;
            escaped = false;
          }
          ch = L'{';
        }
        else {
          // Skip the comment's body
          for ( ;; ) {
            nextChar();

            // Is it the end?
            if ( ch == L'}' && !escaped ) {
              nextChar();

              if ( ch == L'}' && !escaped ) {
                break;
              }
            }
          }

          continue;
        }
      } // if ( ch == '{' )

      // If we're here, we've got a normal symbol, to be saved as text.

      // If there's currently no text node, open one
      if ( !textNode ) {
        Node text = Node( Node::Text(), std::u32string() );

        if ( stack.empty() ) {
          root.push_back( text );
          stack.push_back( &root.back() );
        }
        else {
          stack.back()->push_back( text );
          stack.push_back( &stack.back()->back() );
        }

        textNode = stack.back();
      }

      // If we're inside the transcription, do old-encoding conversion
      if ( transcriptionCount ) {
        switch ( ch ) {
          case 0x2021:
            ch = 0xE6;
            break;
          case 0x407:
            ch = 0x72;
            break;
          case 0xB0:
            ch = 0x6B;
            break;
          case 0x20AC:
            ch = 0x254;
            break;
          case 0x404:
            ch = 0x7A;
            break;
          case 0x40F:
            ch = 0x283;
            break;
          case 0xAB:
            ch = 0x74;
            break;
          case 0xAC:
            ch = 0x64;
            break;
          case 0x2020:
            ch = 0x259;
            break;
          case 0x490:
            ch = 0x6D;
            break;
          case 0xA7:
            ch = 0x66;
            break;
          case 0xAE:
            ch = 0x6C;
            break;
          case 0xB1:
            ch = 0x67;
            break;
          case 0x45E:
            ch = 0x65;
            break;
          case 0xAD:
            ch = 0x6E;
            break;
          case 0xA9:
            ch = 0x73;
            break;
          case 0xA6:
            ch = 0x77;
            break;
          case 0x2026:
            ch = 0x28C;
            break;
          case 0x452:
            ch = 0x76;
            break;
          case 0x408:
            ch = 0x70;
            break;
          case 0x40C:
            ch = 0x75;
            break;
          case 0x406:
            ch = 0x68;
            break;
          case 0xB5:
            ch = 0x61;
            break;
          case 0x491:
            ch = 0x25B;
            break;
          case 0x40A:
            ch = 0x14B;
            break;
          case 0x2030:
            ch = 0xF0;
            break;
          case 0x456:
            ch = 0x6A;
            break;
          case 0xA4:
            ch = 0x62;
            break;
          case 0x409:
            ch = 0x292;
            break;
          case 0x40E:
            ch = 0x69;
            break;
          //case 0x44D: ch = 0x131; break;
          case 0x40B:
            ch = 0x4E8;
            break;
          case 0xB6:
            ch = 0x28A;
            break;
          case 0x2018:
            ch = 0x251;
            break;
          case 0x457:
            ch = 0x265;
            break;
          case 0x458:
            ch = 0x153;
            break;
          case 0x405:
            textNode->text.push_back( 0x153 );
            ch = 0x303;
            break;
          case 0x441:
            ch = 0x272;
            break;
          case 0x442:
            textNode->text.push_back( 0x254 );
            ch = 0x303;
            break;
          case 0x443:
            ch = 0xF8;
            break;
          case 0x445:
            textNode->text.push_back( 0x25B );
            ch = 0x303;
            break;
          case 0x446:
            ch = 0xE7;
            break;
          case 0x44C:
            textNode->text.push_back( 0x251 );
            ch = 0x303;
            break;
          case 0x44D:
            ch = 0x26A;
            break;
          case 0x44F:
            ch = 0x252;
            break;
          case 0x30:
            ch = 0x3B2;
            break;
          case 0x31:
            textNode->text.push_back( 0x65 );
            ch = 0x303;
            break;
          case 0x32:
            ch = 0x25C;
            break;
          case 0x33:
            ch = 0x129;
            break;
          case 0x34:
            ch = 0xF5;
            break;
          case 0x36:
            ch = 0x28E;
            break;
          case 0x37:
            ch = 0x263;
            break;
          case 0x38:
            ch = 0x1DD;
            break;
          case 0x3A:
            ch = 0x2D0;
            break;
          case 0x27:
            ch = 0x2C8;
            break;
          case 0x455:
            ch = 0x1D0;
            break;
          case 0xB7:
            ch = 0xE3;
            break;

          case 0x00a0:
            ch = 0x02A7;
            break;
          //case 0x00b1: ch = 0x0261; break;
          case 0x0402:
            textNode->text.push_back( 0x0069 );
            ch = L':';
            break;
          case 0x0403:
            textNode->text.push_back( 0x0251 );
            ch = L':';
            break;
          //case 0x040b: ch = 0x03b8; break;
          //case 0x040e: ch = 0x026a; break;
          case 0x0428:
            ch = 0x0061;
            break;
          case 0x0453:
            textNode->text.push_back( 0x0075 );
            ch = L':';
            break;
          case 0x201a:
            ch = 0x0254;
            break;
          case 0x201e:
            ch = 0x0259;
            break;
          case 0x2039:
            textNode->text.push_back( 0x0064 );
            ch = 0x0292;
            break;
        }
      }

      if ( escaped && ch == L' ' && mediaCount == 0 ) {
        ch = 0xA0; // Escaped spaces turn into non-breakable ones in Lingvo
      }

      textNode->text.push_back( ch );
    } // for( ; ; )
  }
  catch ( eot & ) {
  }

  if ( textNode ) {
    stack.pop_back();
  }

  if ( !stack.empty() ) {
    auto it = std::find_if( stack.begin(), stack.end(), MustTagBeClosed() );
    if ( it == stack.end() ) {
      return; // no unclosed tags that must be closed => nothing to warn about
    }
    const QByteArray firstTagName = QString::fromStdU32String( ( *it )->tagName ).toUtf8();
    ++it;
    const unsigned unclosedTagCount = 1 + std::count_if( it, stack.end(), MustTagBeClosed() );

    if ( dictName.empty() ) {
      qWarning( "Warning: %u tag(s) were unclosed, first tag name \"%s\".",
                unclosedTagCount,
                firstTagName.constData() );
    }
    else {
      qWarning( "Warning: %u tag(s) were unclosed in \"%s\", article \"%s\", first tag name \"%s\".",
                unclosedTagCount,
                dictName.c_str(),
                QString::fromStdU32String( headword ).toUtf8().constData(),
                firstTagName.constData() );
    }
  }
}

void ArticleDom::openTag( const std::u32string & name, const std::u32string & attrs, list< Node * > & stack )
{
  list< Node > nodesToReopen;

  if ( isAnyM( name ) ) {
    // All tags above [m] tag will be closed and reopened after
    // to avoid break this tag by closing some other tag.

    while ( !stack.empty() ) {
      nodesToReopen.emplace_back( Node::Tag(), stack.back()->tagName, stack.back()->tagAttrs );

      if ( stack.back()->empty() ) {
        // Empty nodes are deleted since they're no use

        stack.pop_back();

        Node * parent = !stack.empty() ? stack.back() : &root;

        parent->pop_back();
      }
      else {
        stack.pop_back();
      }
    }
  }

  // Add tag

  Node node( Node::Tag(), name, attrs );

  if ( stack.empty() ) {
    root.push_back( node );
    stack.push_back( &root.back() );
  }
  else {
    stack.back()->push_back( node );
    stack.push_back( &stack.back()->back() );
  }

  // Reopen tags if needed

  while ( !nodesToReopen.empty() ) {
    if ( stack.empty() ) {
      root.push_back( nodesToReopen.back() );
      stack.push_back( &root.back() );
    }
    else {
      stack.back()->push_back( nodesToReopen.back() );
      stack.push_back( &stack.back()->back() );
    }

    nodesToReopen.pop_back();
  }
}

void ArticleDom::closeTag( const std::u32string & name, list< Node * > & stack, bool warn )
{
  // Find the tag which is to be closed

  list< Node * >::reverse_iterator n;

  for ( n = stack.rbegin(); n != stack.rend(); ++n ) {
    if ( ( *n )->tagName == name || checkM( ( *n )->tagName, name ) ) {
      // Found it
      break;
    }
  }

  if ( n != stack.rend() ) {
    // If there is a corresponding tag, close all tags above it,
    // then close the tag itself

    while ( !stack.empty() ) {
      bool found = stack.back()->tagName == name || checkM( stack.back()->tagName, name );

      if ( stack.back()->empty() && stack.back()->tagName != U"br" ) {
        // Empty nodes except [br] tag are deleted since they're no use

        stack.pop_back();

        Node * parent = !stack.empty() ? stack.back() : &root;

        parent->pop_back();
      }
      else {
        stack.pop_back();
      }

      if ( found ) {
        break;
      }
    }
  }
  else if ( warn ) {
    if ( !dictionaryName.empty() ) {
      qWarning( R"(No corresponding opening tag for closing tag "%s" found in "%s", article "%s".)",
                QString::fromStdU32String( name ).toUtf8().data(),
                dictionaryName.c_str(),
                QString::fromStdU32String( headword ).toUtf8().data() );
    }
    else {
      qWarning( "No corresponding opening tag for closing tag \"%s\" found.",
                QString::fromStdU32String( name ).toUtf8().data() );
    }
  }
}

void ArticleDom::nextChar()
{
  if ( !*stringPos ) {
    throw eot();
  }

  ch = *stringPos++;

  if ( ch == L'\\' ) {
    if ( !*stringPos ) {
      throw eot();
    }

    ch = *stringPos++;

    escaped = true;
  }
  else if ( ch == L'[' && *stringPos == L'[' ) {
    ++stringPos;
    escaped = true;
  }
  else if ( ch == L']' && *stringPos == L']' ) {
    ++stringPos;
    escaped = true;
  }
  else {
    escaped = false;
  }

  if ( ch == '\n' || ch == '\r' ) {
    lineStartPos = stringPos;
  }
}

bool ArticleDom::atSignFirstInLine()
{
  // Check if '@' sign is first after '\n', leading spaces and dsl tags
  if ( stringPos <= lineStartPos ) {
    return true;
  }

  return isAtSignFirst( std::u32string( lineStartPos ) );
}

/////////////// DslScanner

DslScanner::DslScanner( const string & fileName ):
  encoding( Text::Encoding::Utf8 ),
  readBufferPtr( readBuffer ),
  readBufferLeft( 0 ),
  linesRead( 0 )
{
  // Since .dz is backwards-compatible with .gz, we use gz- functions to
  // read it -- they are much nicer than the dict_data- ones.

  f = gd_gzopen( fileName.c_str() );
  if ( !f ) {
    throw exCantOpen( fileName );
  }

  // Now try guessing the encoding

  constexpr size_t firstBytesSize = 50;
  unsigned char firstBytes[ firstBytesSize ];

  if ( gzread( f, firstBytes, firstBytesSize ) != firstBytesSize ) {
    // Apparently the file's too short
    gzclose( f );
    throw exMalformedDslFile( fileName );
  }

  bool needExactEncoding = false;

  // Note that .dsl format always starts with "#NAME"
  if ( auto guessedEncoding = QStringConverter::encodingForData( { firstBytes, firstBytesSize }, '#' );
       guessedEncoding.has_value() ) {
    switch ( guessedEncoding.value() ) {
      case QStringConverter::Utf8:
        encoding = Text::Encoding::Utf8;
        break;
      case QStringConverter::Utf16LE:
        encoding = Text::Encoding::Utf16LE;
        break;
      case QStringConverter::Utf16BE:
        encoding = Text::Encoding::Utf16BE;
        break;
      case QStringConverter::Utf32LE:
        encoding = Text::Encoding::Utf16LE;
        break;
      case QStringConverter::Utf32BE:
        encoding = Text::Encoding::Utf32BE;
        break;
      default:
        break;
    }
  }

  qDebug() << "DSL encoding:" << getEncodingNameFor( encoding );

  if ( gzrewind( f ) ) {
    gzclose( f );
    throw exCantOpen( fileName );
  }

  //iconv.reinit( encoding );
  lineFeed = Text::initLineFeed( encoding );
  // We now can use our own readNextLine() function

  std::u32string str;
  size_t offset;

  for ( ;; ) {
    if ( !readNextLine( str, offset ) ) {
      gzclose( f );
      throw exMalformedDslFile( fileName );
    }

    if ( str.empty() || str[ 0 ] != L'#' ) {
      break;
    }

    bool isName      = false;
    bool isLangFrom  = false;
    bool isLangTo    = false;
    bool isSoundDict = false;

    if ( !str.compare( 0, 5, U"#NAME", 5 ) ) {
      isName = true;
    }
    else if ( !str.compare( 0, 15, U"#INDEX_LANGUAGE", 15 ) ) {
      isLangFrom = true;
    }
    else if ( !str.compare( 0, 18, U"#CONTENTS_LANGUAGE", 18 ) ) {
      isLangTo = true;
    }
    else if ( !str.compare( 0, 17, U"#SOUND_DICTIONARY", 17 ) ) {
      isSoundDict = true;
    }
    else if ( str.compare( 0, 17, U"#SOURCE_CODE_PAGE", 17 ) ) {
      continue;
    }

    // Locate the argument

    size_t beg = str.find_first_of( L'"' );

    if ( beg == std::u32string::npos ) {
      throw exMalformedDslFile( fileName );
    }

    size_t end = str.find_last_of( L'"' );

    if ( end == beg ) {
      throw exMalformedDslFile( fileName );
    }

    std::u32string arg( str, beg + 1, end - beg - 1 );

    if ( isName ) {
      dictionaryName = arg;
    }
    else if ( isLangFrom ) {
      langFrom = arg;
    }
    else if ( isLangTo ) {
      langTo = arg;
    }
    else if ( isSoundDict ) {
      soundDictionary = arg;
    }
    else {
      // The encoding
      if ( !needExactEncoding ) {
        // We don't need that!
        qWarning( "Warning: encoding was specified in a Unicode file, ignoring." );
      }
      else if ( !arg.compare( U"Latin" ) ) {
        encoding = Text::Encoding::Windows1252;
      }
      else if ( !arg.compare( U"Cyrillic" ) ) {
        encoding = Text::Encoding::Windows1251;
      }
      else if ( !arg.compare( U"EasternEuropean" ) ) {
        encoding = Text::Encoding::Windows1250;
      }
      else {
        gzclose( f );
        throw exUnknownCodePage();
      }
    }
  }

  // The loop will always end up reading a line which was not a #-directive.
  // We need to rewind to that line so readNextLine() would return it again
  // next time it's called. To do that, we just use the slow gzseek() and
  // empty the read buffer.
  if ( gzdirect( f ) ) { // Without this ZLib 1.2.7 gzread() return 0
    gzrewind( f );       // after gzseek() call on uncompressed files
  }
  gzseek( f, offset, SEEK_SET );
  readBufferPtr  = readBuffer;
  readBufferLeft = 0;
}

DslScanner::~DslScanner() noexcept
{
  gzclose( f );
}

bool DslScanner::readNextLine( std::u32string & out, size_t & offset, bool only_head_word )
{
  offset = gztell( f ) - readBufferLeft /*+pos*/;

  for ( ;; ) {
    // Check that we have bytes to read
    if ( readBufferLeft < 5000 ) {
      if ( !gzeof( f ) ) {
        // To avoid having to deal with ring logic, we move the remaining bytes
        // to the beginning
        memmove( readBuffer, readBufferPtr, readBufferLeft );

        // Read some more bytes to readBuffer
        const int result = gzread( f, readBuffer + readBufferLeft, sizeof( readBuffer ) - readBufferLeft );

        if ( result == -1 ) {
          throw exCantReadDslFile();
        }

        readBufferPtr = readBuffer;
        readBufferLeft += (size_t)result;
      }
    }
    if ( readBufferLeft <= 0 ) {
      return false;
    }

    int pos = Text::findFirstLinePosition( readBufferPtr, readBufferLeft, lineFeed.lineFeed, lineFeed.length );
    if ( pos == -1 ) {
      return false;
    }
    QString line = Iconv::toQString( getEncodingNameFor( encoding ), readBufferPtr, pos );
    line         = Utils::rstrip( line );

    if ( pos > readBufferLeft ) {
      pos = readBufferLeft;
    }
    readBufferLeft -= pos;
    readBufferPtr += pos;
    linesRead++;
    if ( only_head_word && ( line.isEmpty() || line.at( 0 ).isSpace() ) ) {
      continue;
    }
    out = line.toStdU32String();
    return true;
  }
}

bool DslScanner::readNextLineWithoutComments( std::u32string & out, size_t & offset, bool only_headword )
{
  std::u32string str;
  bool commentToNextLine = false;
  size_t currentOffset;

  out.erase();
  offset = 0;

  do {
    bool b = readNextLine( str, currentOffset, only_headword );

    if ( offset == 0 ) {
      offset = currentOffset;
    }

    if ( !b ) {
      return false;
    }

    stripComments( str, commentToNextLine );

    out += str;
  } while ( commentToNextLine );

  return true;
}

/////////////// DslScanner

void processUnsortedParts( std::u32string & str, bool strip )
{
  int refCount = 0;

  size_t startPos = 0;

  for ( size_t x = 0; x < str.size(); ) {
    char32_t ch = str[ x ];

    if ( ch == L'\\' ) {
      // Escape code
      x += 2;
      continue;
    }

    if ( ch == '{' ) {
      ++refCount;

      if ( !strip ) {
        // Just remove it and continue
        str.erase( x, 1 );
        continue;
      }
      else if ( refCount == 1 ) {
        // First opening brace. Save this position, we will be erasing the
        // whole range when we encounter the last closing brace.
        startPos = x;
      }
    }
    else if ( ch == '}' ) {
      --refCount;

      if ( refCount < 0 ) {
        qWarning( "Warning: an unmatched closing brace was encountered." );
        refCount = 0;
        // But we remove that thing either way
        str.erase( x, 1 );
        continue;
      }

      if ( !strip ) {
        // Just remove it and continue
        str.erase( x, 1 );
        continue;
      }
      else if ( !refCount ) {
        // The final closing brace -- we can erase the whole range now.
        str.erase( startPos, x - startPos + 1 );
        x = startPos;

        continue;
      }
    }

    ++x;
  }

  if ( strip && refCount ) {
    qWarning( "Warning: unclosed brace(s) encountered." );
    str.erase( startPos );
  }
}

void expandOptionalParts( std::u32string & str, list< std::u32string > * result, size_t x, bool inside_recurse )
{
  // if str is too long ,it can never be headwords.
  if ( str.size() > 100 ) {
    return;
  }
  list< std::u32string > expanded;
  list< std::u32string > * headwords;
  headwords = inside_recurse ? result : &expanded;

  for ( ; x < str.size(); ) {
    char32_t ch = str[ x ];

    if ( ch == L'\\' ) {
      // Escape code
      x += 2;
    }
    else if ( ch == L'(' ) {
      // First, handle the case where this block is removed

      {
        int refCount = 1;

        for ( size_t y = x + 1; y < str.size(); ++y ) {
          char32_t ch = str[ y ];

          if ( ch == L'\\' ) {
            // Escape code
            ++y;
          }
          else if ( ch == L'(' ) {
            ++refCount;
          }
          else if ( ch == L')' ) {
            if ( !--refCount ) {
              // Now that the closing parenthesis is found,
              // cut the whole thing out and be done.

              if ( y != x + 1 ) // Only do for non-empty cases
              {
                std::u32string removed( str, 0, x );
                removed.append( str, y + 1, str.size() - y - 1 );

                expandOptionalParts( removed, headwords, x, true );
              }

              break;
            }
          }
        }

        if ( refCount && x != str.size() - 1 ) {
          // Closing paren not found? Chop it.

          std::u32string removed( str, 0, x );

          // Limit the amount of results to avoid excessive resource consumption
          if ( headwords->size() < 32 ) {
            headwords->push_back( removed );
          }
          else {
            if ( !inside_recurse ) {
              result->splice( result->end(), expanded );
            }
            return;
          }
        }
      }

      // Now, handling the case where it is kept -- we just erase
      // the paren and go on

      str.erase( x, 1 );
    }
    else if ( ch == L')' ) {
      // Closing paren doesn't mean much -- just erase it
      str.erase( x, 1 );
    }
    else {
      ++x;
    }
  }

  // Limit the amount of results to avoid excessive resource consumption
  if ( headwords->size() < 32 ) {
    headwords->push_back( str );
  }
  if ( !inside_recurse ) {
    result->splice( result->end(), expanded );
  }
}

static const std::u32string openBraces( U"{{" );
static const std::u32string closeBraces( U"}}" );

void stripComments( std::u32string & str, bool & nextLine )
{
  string::size_type n = 0, n2 = 0;

  for ( ;; ) {
    if ( nextLine ) {
      n = str.find( closeBraces, n2 );
      if ( n == string::npos ) {
        str.erase( n2, n );
        return;
      }
      str.erase( n2, n - n2 + 2 );
      nextLine = false;
    }

    n = str.find( openBraces, n2 );
    if ( n == string::npos ) {
      return;
    }
    nextLine = true;
    n2       = n;
  }
}

void expandTildes( std::u32string & str, const std::u32string & tildeReplacement )
{
  std::u32string tildeValue = Folding::trimWhitespace( tildeReplacement );
  for ( size_t x = 0; x < str.size(); ) {
    if ( str[ x ] == L'\\' ) {
      x += 2;
    }
    else if ( str[ x ] == L'~' ) {
      if ( x > 0 && str[ x - 1 ] == '^' && ( x < 2 || str[ x - 2 ] != '\\' ) ) {
        str.replace( x - 1, 2, tildeValue );
        str[ x - 1 ] =
          QChar( str[ x - 1 ] ).isUpper() ? QChar::toLower( str[ x - 1 ] ) : QChar::toUpper( str[ x - 1 ] );
        x = x - 1 + tildeValue.size();
      }
      else {
        str.replace( x, 1, tildeValue );
        x += tildeValue.size();
      }
    }
    else {
      ++x;
    }
  }
}

void unescapeDsl( std::u32string & str )
{
  for ( size_t x = 0; x < str.size(); ++x ) {
    if ( str[ x ] == L'\\' ) {
      str.erase( x, 1 ); // ++x would skip the next char without processing it
    }
  }
}

void normalizeHeadword( std::u32string & str )
{
  for ( size_t x = str.size(); x-- > 1; ) // >1 -- Don't test the first char
  {
    if ( str[ x ] == L' ' ) {
      size_t y;
      for ( y = x; y && ( str[ y - 1 ] == L' ' ); --y ) {
        ;
      }

      if ( y != x ) {
        // Remove extra spaces

        str.erase( y, x - y );

        x = y;
      }
    }
  }
  if ( !str.empty() && str[ str.size() - 1 ] == L' ' ) {
    str.erase( str.size() - 1, 1 );
  }
  if ( !str.empty() && str[ 0 ] == L' ' ) {
    str.erase( 0, 1 );
  }
}

namespace {
void cutEnding( std::u32string & where, const std::u32string & ending )
{
  if ( where.size() > ending.size() && where.compare( where.size() - ending.size(), ending.size(), ending ) == 0 ) {
    where.erase( where.size() - ending.size() );
  }
}
} // namespace

quint32 dslLanguageToId( const std::u32string & name )
{
  static std::u32string newSp( U"newspelling" );
  static std::u32string st( U"standard" );
  static std::u32string ms( U"modernsort" );
  static std::u32string ts( U"traditionalsort" );
  static std::u32string prc( U"prc" );

  // Any of those endings are to be removed

  std::u32string nameStripped = Folding::apply( name );

  cutEnding( nameStripped, newSp );
  cutEnding( nameStripped, st );
  cutEnding( nameStripped, ms );
  cutEnding( nameStripped, ts );
  cutEnding( nameStripped, prc );

  return LangCoder::findIdForLanguage( nameStripped );
}

} // namespace Details
} // namespace Dsl
