#include "dictionary_icon_name.hh"
#include <QMutexLocker>


QString Icons::DictionaryIconName::getIconName( const QString & key, const QString & nameText )
{
  if ( key.isEmpty() ) {
    return {};
  }
  QMutexLocker _( &_mutex );

  if ( _dictionaryIconNames.contains( key ) ) {
    return _dictionaryIconNames.value( key );
  }
  // Use nameText for character extraction. If nameText is empty, fallback to key only if it's likely a name.
  // But preferably, nameText should be the simplified dictionary name.
  QString source = nameText.isEmpty() ? key : nameText;
  if ( source.isEmpty() ) {
    return {};
  }
  QString name = source.at( 0 ).toUpper();
  auto it1     = _iconDictionaryNames.contains( name );
  if ( it1 ) {
    auto vector = _iconDictionaryNames.value( name );
    vector++;
    _iconDictionaryNames.insert( name, vector );
  }
  else {
    _iconDictionaryNames.insert( name, 1 );
  }

  name = name + QString::number( _iconDictionaryNames.value( name ) );
  _dictionaryIconNames.insert( key, name );
  return name;
}
