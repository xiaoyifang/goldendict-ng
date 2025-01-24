/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "folding.hh"

#include "text.hh"
#include "globalregex.hh"
#include "inc_case_folding.hh"

namespace Folding {

/// Tests if the given char is one of the Unicode combining marks. Some are
/// caught by the diacritics folding table, but they are only handled there
/// when they come with their main characters, not by themselves. The rest
/// are caught here.
bool isCombiningMark( char32_t ch )
{
  return QChar::isMark( ch );
}

std::u32string apply( std::u32string const & in, bool preserveWildcards )
{
  // remove diacritics (normalization), white space, punt,
  auto temp = QString::fromStdU32String( in )
                .normalized( QString::NormalizationForm_KD )
                .remove( RX::markSpace )
                .removeIf( [ preserveWildcards ]( const QChar & ch ) -> bool {
                  return ch.isPunct()
                    && !( preserveWildcards && ( ch == '\\' || ch == '?' || ch == '*' || ch == '[' || ch == ']' ) );
                } )
                .toStdU32String();
  // case folding
  std::u32string caseFolded;
  caseFolded.reserve( temp.size() );
  char32_t buf[ foldCaseMaxOut ];
  for ( const char32_t ch : temp ) {
    auto n = foldCase( ch, buf );
    caseFolded.append( buf, n );
  }
  return caseFolded;
}

std::u32string applySimpleCaseOnly( std::u32string const & in )
{
  char32_t const * nextChar = in.data();

  std::u32string out;

  out.reserve( in.size() );

  for ( size_t left = in.size(); left--; ) {
    out.push_back( foldCaseSimple( *nextChar++ ) );
  }

  return out;
}

std::u32string applySimpleCaseOnly( QString const & in )
{
  //qt only support simple case folding.
  return in.toCaseFolded().toStdU32String();
}

std::u32string applySimpleCaseOnly( std::string const & in )
{
  return applySimpleCaseOnly( Text::toUtf32( in ) );
  // return QString::fromStdString( in ).toCaseFolded().toStdU32String();
}

std::u32string applyFullCaseOnly( std::u32string const & in )
{
  std::u32string caseFolded;

  caseFolded.reserve( in.size() * foldCaseMaxOut );

  char32_t const * nextChar = in.data();

  char32_t buf[ foldCaseMaxOut ];

  for ( size_t left = in.size(); left--; ) {
    caseFolded.append( buf, foldCase( *nextChar++, buf ) );
  }

  return caseFolded;
}

std::u32string applyDiacriticsOnly( std::u32string const & in )
{
  auto noAccent = QString::fromStdU32String( in ).normalized( QString::NormalizationForm_KD ).remove( RX::accentMark );
  return noAccent.toStdU32String();
}

std::u32string applyPunctOnly( std::u32string const & in )
{
  char32_t const * nextChar = in.data();

  std::u32string out;

  out.reserve( in.size() );

  for ( size_t left = in.size(); left--; ++nextChar ) {
    if ( !isPunct( *nextChar ) ) {
      out.push_back( *nextChar );
    }
  }

  return out;
}

QString applyPunctOnly( QString const & in )
{
  QString out;
  for ( auto c : in ) {
    if ( !c.isPunct() ) {
      out.push_back( c );
    }
  }

  return out;
}

std::u32string applyWhitespaceOnly( std::u32string const & in )
{
  char32_t const * nextChar = in.data();

  std::u32string out;

  out.reserve( in.size() );

  for ( size_t left = in.size(); left--; ++nextChar ) {
    if ( !isWhitespace( *nextChar ) ) {
      out.push_back( *nextChar );
    }
  }

  return out;
}

std::u32string applyWhitespaceAndPunctOnly( std::u32string const & in )
{
  char32_t const * nextChar = in.data();

  std::u32string out;

  out.reserve( in.size() );

  for ( size_t left = in.size(); left--; ++nextChar ) {
    if ( !isWhitespaceOrPunct( *nextChar ) ) {
      out.push_back( *nextChar );
    }
  }

  return out;
}

bool isWhitespace( char32_t ch )
{
  return QChar::isSpace( ch );
}

bool isWhitespaceOrPunct( char32_t ch )
{
  return isWhitespace( ch ) || QChar::isPunct( ch );
}

bool isPunct( char32_t ch )
{
  return QChar::isPunct( ch );
}

std::u32string trimWhitespaceOrPunct( std::u32string const & in )
{
  char32_t const * wordBegin         = in.c_str();
  std::u32string::size_type wordSize = in.size();

  // Skip any leading whitespace
  while ( *wordBegin && Folding::isWhitespaceOrPunct( *wordBegin ) ) {
    ++wordBegin;
    --wordSize;
  }

  // Skip any trailing whitespace
  while ( wordSize && Folding::isWhitespaceOrPunct( wordBegin[ wordSize - 1 ] ) ) {
    --wordSize;
  }

  return std::u32string( wordBegin, wordSize );
}

QString trimWhitespaceOrPunct( QString const & in )
{
  auto wordSize = in.size();

  int wordBegin = 0;
  int wordEnd   = wordSize;
  // Skip any leading whitespace
  while ( wordBegin < wordSize && ( in[ wordBegin ].isSpace() || in[ wordBegin ].isPunct() ) ) {
    ++wordBegin;
    wordSize--;
  }

  // Skip any trailing whitespace
  while ( wordEnd > 0 && ( in[ wordEnd - 1 ].isSpace() || in[ wordEnd - 1 ].isPunct() ) ) {
    --wordEnd;
    wordSize--;
  }

  return in.mid( wordBegin, wordSize );
}

std::u32string trimWhitespace( std::u32string const & in )
{
  if ( in.empty() ) {
    return in;
  }
  char32_t const * wordBegin         = in.c_str();
  std::u32string::size_type wordSize = in.size();

  // Skip any leading whitespace
  while ( *wordBegin && Folding::isWhitespace( *wordBegin ) ) {
    ++wordBegin;
    --wordSize;
  }

  // Skip any trailing whitespace
  while ( wordSize && Folding::isWhitespace( wordBegin[ wordSize - 1 ] ) ) {
    --wordSize;
  }

  return std::u32string( wordBegin, wordSize );
}

QString trimWhitespace( QString const & in )
{
  return in.trimmed();
}

QString escapeWildcardSymbols( const QString & str )
{
  QString escaped( str );
  escaped.replace( QRegularExpression( R"(([\[\]\?\*]))" ), R"(\\1)" );

  return escaped;
}

QString unescapeWildcardSymbols( const QString & str )
{
  QString unescaped( str );
  unescaped.replace( QRegularExpression( R"(\\([\[\]\?\*]))" ), R"(\1)" );

  return unescaped;
}
} // namespace Folding
