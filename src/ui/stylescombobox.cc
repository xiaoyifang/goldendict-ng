/* Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "stylescombobox.hh"
#include "config.hh"
#include <QDir>

StylesComboBox::StylesComboBox( QWidget * parent ):
  QComboBox( parent )
{
  fill();
}

void StylesComboBox::fill()
{
  clear();
  addItem( tr( "None" ) );
  QString stylesDir = Config::getStylesDir();
  if ( !stylesDir.isEmpty() ) {
    QDir dir( stylesDir );
    QStringList styles = dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::LocaleAware );
    if ( !styles.isEmpty() ) {
      addItems( styles );
    }
  }

  setVisible( count() > 1 );
}

void StylesComboBox::setCurrentStyle( QString const & style )
{
  int nom = findText( style );
  if ( nom > -1 ) {
    setCurrentIndex( nom );
  }
}

QString StylesComboBox::getCurrentStyle() const
{
  if ( currentIndex() == 0 )
    return {};
  return itemText( currentIndex() );
}
