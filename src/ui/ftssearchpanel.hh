#pragma once

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
