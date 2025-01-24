/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <string>
#include <list>
#include <vector>
#include <zlib.h>
#include "dictionary.hh"
#include "iconv.hh"
#include <QByteArray>
#include "text.hh"

// Implementation details for Dsl, not part of its interface
namespace Dsl {
namespace Details {

using std::string;
using std::list;
using std::vector;
using Text::Encoding;
using Text::LineFeed;

string findCodeForDslId( int id );

bool isAtSignFirst( std::u32string const & str );

/// Parses the DSL language, representing it in its structural DOM form.
struct ArticleDom
{
  struct Node: public list< Node >
  {
    bool isTag; // true if it is a tag with subnodes, false if it's a leaf text
                // data.
    // Those are only used if isTag is true
    std::u32string tagName;
    std::u32string tagAttrs;
    std::u32string text; // This is only used if isTag is false

    class Text
    {};
    class Tag
    {};

    Node( Tag, std::u32string const & name, std::u32string const & attrs ):
      isTag( true ),
      tagName( name ),
      tagAttrs( attrs )
    {
    }

    Node( Text, std::u32string const & text_ ):
      isTag( false ),
      text( text_ )
    {
    }

    /// Concatenates all childen text nodes recursively to form all text
    /// the node contains stripped of any markup.
    std::u32string renderAsText( bool stripTrsTag = false ) const;
  };

  /// Does the parse at construction. Refer to the 'root' member variable
  /// afterwards.
  explicit ArticleDom( std::u32string const &,
                       string const & dictName          = string(),
                       std::u32string const & headword_ = std::u32string() );

  /// Root of DOM's tree
  Node root;

private:

  void openTag( std::u32string const & name, std::u32string const & attr, list< Node * > & stack );

  void closeTag( std::u32string const & name, list< Node * > & stack, bool warn = true );

  bool atSignFirstInLine();

  char32_t const *stringPos, *lineStartPos;

  class eot: std::exception
  {};

  char32_t ch;
  bool escaped;
  unsigned transcriptionCount; // >0 = inside a [t] tag
  unsigned mediaCount;         // >0 = inside a [s] tag

  void nextChar();

  /// Information for diagnostic purposes
  string dictionaryName;
  std::u32string headword;
};

/// Opens the .dsl or .dsl.dz file and allows line-by-line reading. Auto-detects
/// the encoding, and reads all headers by itself.
class DslScanner
{
  gzFile f;
  Encoding encoding;
  std::u32string dictionaryName;
  std::u32string langFrom, langTo;
  std::u32string soundDictionary;
  char readBuffer[ 65536 ];
  char * readBufferPtr;
  LineFeed lineFeed;
  size_t readBufferLeft;
  //qint64 pos;
  unsigned linesRead;

public:

  DEF_EX( Ex, "Dsl scanner exception", Dictionary::Ex )
  DEF_EX_STR( exCantOpen, "Can't open .dsl file", Ex )
  DEF_EX( exCantReadDslFile, "Can't read .dsl file", Ex )
  DEF_EX_STR( exMalformedDslFile, "The .dsl file is malformed:", Ex )
  DEF_EX( exUnknownCodePage, "The .dsl file specified an unknown code page", Ex )
  DEF_EX( exEncodingError, "Encoding error", Ex ) // Should never happen really

  explicit DslScanner( string const & fileName );
  ~DslScanner() noexcept;

  /// Returns the detected encoding of this file.
  Encoding getEncoding() const
  {
    return encoding;
  }

  /// Returns the dictionary's name, as was read from file's headers.
  std::u32string const & getDictionaryName() const
  {
    return dictionaryName;
  }

  /// Returns the dictionary's source language, as was read from file's headers.
  std::u32string const & getLangFrom() const
  {
    return langFrom;
  }

  /// Returns the dictionary's target language, as was read from file's headers.
  std::u32string const & getLangTo() const
  {
    return langTo;
  }

  /// Returns the preferred external dictionary with sounds, as was read from file's headers.
  std::u32string const & getSoundDictionaryName() const
  {
    return soundDictionary;
  }

  /// Reads next line from the file. Returns true if reading succeeded --
  /// the string gets stored in the one passed, along with its physical
  /// file offset in the file (the uncompressed one if the file is compressed).
  /// If end of file is reached, false is returned.
  /// Reading begins from the first line after the headers (ones which start
  /// with #).
  bool readNextLine( std::u32string &, size_t & offset, bool only_head_word = false );

  /// Similar readNextLine but strip all DSL comments {{...}}
  bool readNextLineWithoutComments( std::u32string &, size_t & offset, bool only_headword = false );

  /// Returns the number of lines read so far from the file.
  unsigned getLinesRead() const
  {
    return linesRead;
  }

  /// Converts the given number of characters to the number of bytes they
  /// would occupy in the file, knowing its encoding. It's possible to know
  /// that because no multibyte encodings are supported in .dsls.
  inline size_t distanceToBytes( size_t ) const;
};

/// This function either removes parts of string enclosed in braces, or leaves
/// them intact. The braces themselves are removed always, though.
void processUnsortedParts( std::u32string & str, bool strip );

/// Expands optional parts of a headword (ones marked with parentheses),
/// producing all possible combinations where they are present or absent.
void expandOptionalParts( std::u32string & str,
                          list< std::u32string > * result,
                          size_t x            = 0,
                          bool inside_recurse = false );

/// Expands all unescaped tildes, inserting tildeReplacement text instead of
/// them.
void expandTildes( std::u32string & str, std::u32string const & tildeReplacement );

/// Unescapes any escaped chars. Be sure to handle all their special meanings
/// before unescaping them.
void unescapeDsl( std::u32string & str );

/// Normalizes the headword. Currently turns any sequences of consecutive spaces
/// into a single space.
void normalizeHeadword( std::u32string & );

/// Strip DSL {{...}} comments
void stripComments( std::u32string &, bool & );

inline size_t DslScanner::distanceToBytes( size_t x ) const
{
  switch ( encoding ) {
    case Encoding::Utf16LE:
    case Encoding::Utf16BE:
      return x * 2;
    default:
      return x;
  }
}

/// Converts the given language name taken from Dsl header (i.e. getLangFrom(),
/// getLangTo()) to its proper language id.
quint32 dslLanguageToId( std::u32string const & name );

} // namespace Details
} // namespace Dsl
