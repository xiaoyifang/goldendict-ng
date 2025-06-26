#pragma once


#include <QFontComboBox>
#include <QString>

class CustomFontComboBox: public QFontComboBox
{
  Q_OBJECT

public:
  explicit CustomFontComboBox( QWidget * parent = nullptr );
  void insertCustomItem( int index, const QString & item );
};
