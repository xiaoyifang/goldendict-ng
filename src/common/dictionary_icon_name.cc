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
  // Get the next index for this character (e.g., T1, T2, T3)
  // operator[] returns a reference and default-initializes to 0 if not exist.
  int charCount = ++_iconDictionaryNames[name];

  QString resultName = name + QString::number( charCount );
  _dictionaryIconNames.insert( key, resultName );
  return resultName;
}
