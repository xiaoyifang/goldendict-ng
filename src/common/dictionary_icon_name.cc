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
  std::vector< QString > vector = {};
  if ( it1 ) {
    vector = _iconDictionaryNames.value( name );
    vector.emplace_back( dictionaryName );
  }
  else {
    vector.emplace_back( dictionaryName );
    _iconDictionaryNames.insert( name, vector );
  }

  name = name + QString::number( vector.size() );
  _dictionaryIconNames.insert( dictionaryName, name );
  return name;
}