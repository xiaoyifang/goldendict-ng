#include "dictionary_icon_name.hh"
#include <QMutexLocker>


QString Icons::DictionaryIconName::getIconName( const QString & dictionaryName )
{
  if ( dictionaryName.isEmpty() ) {
    return {};
  }
  QMutexLocker _( &_mutex );

  auto it = _dictionaryIconNames.find( dictionaryName );
  if ( it != _dictionaryIconNames.end() ) {
    return it->second;
  }
  //get the first character of the dictionary name
  QString name = dictionaryName.at( 0 ).toUpper();
  auto it1     = _iconDictionaryNames.find( name );
  if ( it1 != _iconDictionaryNames.end() ) {
    it1->second.push_back( dictionaryName );
  }
  else {
    _iconDictionaryNames.insert( std::make_pair( name, std::vector< QString >{ dictionaryName } ) );
  }

  name                                          = name + QString::number( it1->second.size() );
  return _dictionaryIconNames[ dictionaryName ] = name;
}