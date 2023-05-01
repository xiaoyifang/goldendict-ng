#ifndef GOLDENDICT_SEARCHPANEL_H
#define GOLDENDICT_SEARCHPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

class SearchPanel: public QWidget
{
  Q_OBJECT
 public:
  explicit SearchPanel( QWidget * parent = nullptr );
  QLineEdit * lineEdit;
  QPushButton * close;
  QPushButton * previous;
  QPushButton * next;
  QCheckBox * highlightAll;
  QCheckBox * caseSensitive;
};


#endif //GOLDENDICT_SEARCHPANEL_H
