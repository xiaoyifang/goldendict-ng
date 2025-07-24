#pragma once

#include "ui_dictinfo.h"
#include "dict/dictionary.hh"
#include "config.hh"

class DictInfo: public QDialog
{
  Q_OBJECT

public:

  enum Actions {
    REJECTED,
    ACCEPTED,
    OPEN_FOLDER,
    SHOW_HEADWORDS
  };

  DictInfo( Config::Class & cfg_, QWidget * parent = nullptr );
  void showInfo( sptr< Dictionary::Class > dict );

private:
  Ui::DictInfo ui;
  Config::Class & cfg;
private slots:
  void savePos( int );
  void on_openFolder_clicked();
  void on_OKButton_clicked();
  void on_headwordsButton_clicked();
  void on_openIndexFolder_clicked();
};
