/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "folding.hh"
#include <QRegularExpression>

#include "utf8.hh"
#include "globalregex.hh"

namespace Folding {

#include "inc_case_folding.hh"

/// Tests if the given char is one of the Unicode combining marks. Some are
/// caught by the diacritics folding table, but they are only handled there
/// when they come with their main characters, not by themselves. The rest
/// are caught here.
bool isCombiningMark( wchar ch )
{
  return QChar::isMark(ch);
}

wstring apply( wstring const & in, bool preserveWildcards )
{
  //remove space and accent;
  auto withPunc = QString::fromStdU32String( in )
                    .normalized( QString::NormalizationForm_KD )
                    .remove( RX::markSpace )
                    .toStdU32String();

  //First, strip diacritics and apply ws/punctuation removal
  wstring withoutDiacritics;

  withoutDiacritics.reserve( withPunc.size() );


  for ( auto const & ch : withPunc ) {

    if ( !isPunct( ch )
         || ( preserveWildcards && ( ch == '\\' || ch == '?' || ch == '*' || ch == '[' || ch == ']' ) ) ) {
      withoutDiacritics.push_back( ch );
    }
  }


  // Now, fold the case

  wstring caseFolded;

  caseFolded.reserve( withoutDiacritics.size() * foldCaseMaxOut );

  wchar const * nextChar = withoutDiacritics.data();

  wchar buf[ foldCaseMaxOut ];

  for ( size_t left = withoutDiacritics.size(); left--; )
    caseFolded.append( buf, foldCase( *nextChar++, buf ) );

  return caseFolded;
}

wstring applySimpleCaseOnly( wstring const & in )
{
  wchar const * nextChar = in.data();

  wstring out;

  out.reserve( in.size() );

  for( size_t left = in.size(); left--; )
    out.push_back( foldCaseSimple( *nextChar++ ) );

  return out;
}

wstring applySimpleCaseOnly( QString const & in )
{
  //qt only support simple case folding.
  return in.toCaseFolded().toStdU32String();
}

wstring applySimpleCaseOnly( std::string const & in )
{
  return applySimpleCaseOnly( Utf8::decode( in ) );
  // return QString::fromStdString( in ).toCaseFolded().toStdU32String();
}

wstring applyFullCaseOnly( wstring const & in )
{
  wstring caseFolded;

  caseFolded.reserve( in.size() * foldCaseMaxOut );

  wchar const * nextChar = in.data();

  wchar buf[ foldCaseMaxOut ];

  for( size_t left = in.size(); left--; )
    caseFolded.append( buf, foldCase(  *nextChar++, buf ) );

  return caseFolded;
}

wstring applyDiacriticsOnly( wstring const & in )
{
  auto noAccent = QString::fromStdU32String( in ).normalized( QString::NormalizationForm_KD ).remove( RX::accentMark );
  return noAccent.toStdU32String();
}

wstring applyPunctOnly( wstring const & in )
{
  wchar const * nextChar = in.data();

  wstring out;

  out.reserve( in.size() );

  for( size_t left = in.size(); left--; ++nextChar )
    if ( !isPunct( *nextChar ) )
      out.push_back( *nextChar );

  return out;
}

QString applyPunctOnly( QString const & in )
{
  QString out;
  for ( auto c : in )
    if ( !c.isPunct() )
      out.push_back( c );

  return out;
}

wstring applyWhitespaceOnly( wstring const & in )
{
  wchar const * nextChar = in.data();

  wstring out;

  out.reserve( in.size() );

  for( size_t left = in.size(); left--; ++nextChar )
    if ( !isWhitespace( *nextChar ) )
      out.push_back( *nextChar );

  return out;
}

wstring applyWhitespaceAndPunctOnly( wstring const & in )
{
  wchar const * nextChar = in.data();

  wstring out;

  out.reserve( in.size() );

  for( size_t left = in.size(); left--; ++nextChar )
    if ( !isWhitespace( *nextChar ) && !isPunct( *nextChar ) )
      out.push_back( *nextChar );

  return out;
}

bool isWhitespace( wchar ch )
{
  return QChar::isSpace( ch );
}

bool isPunct( wchar ch )
{
  return QChar::isPunct( ch );
}

wstring trimWhitespaceOrPunct( wstring const & in )
{
  wchar const * wordBegin = in.c_str();
  wstring::size_type wordSize = in.size();

  // Skip any leading whitespace
  while( *wordBegin && ( Folding::isWhitespace( *wordBegin ) || Folding::isPunct( *wordBegin ) ) )
  {
    ++wordBegin;
    --wordSize;
  }

  // Skip any trailing whitespace
  while( wordSize && ( Folding::isWhitespace( wordBegin[ wordSize - 1 ] ) ||
                       Folding::isPunct( wordBegin[ wordSize - 1 ] ) ) )
    --wordSize;

  return wstring( wordBegin, wordSize );
}

QString trimWhitespaceOrPunct( QString const & in )
{
  auto wordSize = in.size();

  int wordBegin = 0;
  int wordEnd   = wordSize ;
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

  return in.mid( wordBegin,wordSize );
}

wstring trimWhitespace( wstring const & in )
{
  if( in.empty() )
    return in;
  wchar const * wordBegin = in.c_str();
  wstring::size_type wordSize = in.size();

  // Skip any leading whitespace
  while( *wordBegin && Folding::isWhitespace( *wordBegin ) )
  {
    ++wordBegin;
    --wordSize;
  }

  // Skip any trailing whitespace
  while( wordSize && Folding::isWhitespace( wordBegin[ wordSize - 1 ] ) )
    --wordSize;

  return wstring( wordBegin, wordSize );
}

QString trimWhitespace( QString const & in )
{
  return in.trimmed();
}

QString escapeWildcardSymbols( const QString & str )
{
  QString escaped( str );
  escaped.replace( QRegularExpression( R"(([\[\]\?\*]))" ), "\\\\1" );

  return escaped;
}

QString unescapeWildcardSymbols( const QString & str )
{
  QString unescaped( str );
  unescaped.replace( QRegularExpression( R"(\\([\[\]\?\*]))" ), "\\1" );

  return unescaped;
}
}
