#include "custom.hh"
#include "dictionary.hh"
#include <QCoreApplication>

namespace CustomTranslit {

CustomTransTable::CustomTransTable( const QString & content_ )
{
  parse( content_ );
}

void CustomTransTable::parse( const QString & content )
{
  QTextStream stream( content.toUtf8() );
  while ( !stream.atEnd() ) {
    auto line    = stream.readLine();
    auto hashPos = line.indexOf( '#' );
    if ( hashPos > -1 ) {
      line = line.left( hashPos );
    }

    auto parts = line.split( ';', Qt::SkipEmptyParts );
    if ( parts.size() != 2 ) {
      continue;
    }

    ins( parts[ 0 ].toStdU32String(), parts[ 1 ].toStdU32String() );
  }
}

std::vector< sptr< Dictionary::Class > > makeDictionaries( Config::CustomTrans const & cusTran )
{

  std::vector< sptr< Dictionary::Class > > result;

  if ( cusTran.enable ) {
    static CustomTranslit::CustomTransTable t0( cusTran.context );

    result.push_back( std::make_shared< Transliteration::TransliterationDictionary >(
      "custom-transliteration-dict",
      QCoreApplication::translate( "CustomTranslit", "custom transliteration" ).toUtf8().data(),
      QIcon( ":/icons/custom_trans.svg" ),
      t0,
      true ) );
  }
  return result;
}

} // namespace CustomTranslit
