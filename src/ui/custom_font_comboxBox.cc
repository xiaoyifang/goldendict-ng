#include "customfontcombobox.h"

CustomFontComboBox::CustomFontComboBox( QWidget * parent ):
  QFontComboBox( parent )
{
}

void CustomFontComboBox::insertCustomItem( const QString & item, int index = 0 )
{
  if ( index < 0 || index > this->count() ) {
    index = 0; // Default to the beginning if index is out of bounds
  }
  insertItem( index, item );
}
