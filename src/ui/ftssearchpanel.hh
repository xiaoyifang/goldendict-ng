#ifndef GOLDENDICT_FTSSEARCHPANEL_H
#define GOLDENDICT_FTSSEARCHPANEL_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class FtsSearchPanel: public QWidget
{
  Q_OBJECT

public:
  explicit FtsSearchPanel( QWidget * parent = nullptr );
  QLabel * statusLabel;
  QPushButton * previous;
  QPushButton * next;
};


#endif //GOLDENDICT_FTSSEARCHPANEL_H
