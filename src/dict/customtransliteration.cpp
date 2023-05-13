#include "customtransliteration.hh"
#include "dictionary.hh"

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
    //check part length, normally they should all with length<=2
    bool validState = true;
    for ( auto & part : parts ) {
      if ( part.trimmed().length() > 2 ) {
        validState = false;
        break;
      }
    }

    if ( !validState ) {
      continue;
    }

    ins(parts[0].toUtf8(),parts[1].toUtf8());
  }
}

std::vector< sptr< Dictionary::Class > > makeDictionaries( Config::CustomTrans const & cusTran )
{

  std::vector< sptr< Dictionary::Class > > result;

  if(cusTran.enable){
    static CustomTranslit::CustomTransTable t0(cusTran.context);

    result.push_back(  std::make_shared<Transliteration::TransliterationDictionary>( "custom-transliteration-dict",
                                                                                      QCoreApplication::translate( "CustomTranslit", "custom transliteration" ).toUtf8().data(),
                                                                                      QIcon( ":/icons/custom_trans.svg" ), t0, false ) );
  }
  return result;

}

}
