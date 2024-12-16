#include "dictionary_icon_name.hh"
#include <QMutexLocker>


QString Icons::DictionaryIconName::getIconName( const QString & dictionaryName )
{
  if ( dictionaryName.isEmpty() ) {
    return {};
  }
  QMutexLocker _( &_mutex );

  auto it = _dictionaryIconNames.contains( dictionaryName );
  if ( it ) {
    return _dictionaryIconNames.value( dictionaryName );
  }
  //get the first character of the dictionary name
  QString name = dictionaryName.at( 0 ).toUpper();
  auto it1     = _iconDictionaryNames.contains( name );
  if ( it1 ) {
    _iconDictionaryNames.value( name ).push_back( dictionaryName );
  }
  else {
    _iconDictionaryNames.insert( name, std::vector< QString >{ dictionaryName } );
  }

  name = name + QString::number( _iconDictionaryNames.value( name ).size() );
  _dictionaryIconNames.insert( dictionaryName, name );
  return name;
}