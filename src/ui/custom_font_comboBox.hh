#pragma once


#include <QFontComboBox>
#include <QStringList>

class CustomFontComboBox: public QFontComboBox
{
  Q_OBJECT

public:
  explicit CustomFontComboBox( QWidget * parent = nullptr );
  void insertCustomItem( const QString & item, int index = 0 );
};
