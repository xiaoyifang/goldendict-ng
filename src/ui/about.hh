/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "ui_about.h"
#include "sptr.hh"
#include "dict/dictionary.hh"
#include <QDialog>
#include <vector>

class About: public QDialog
{
  Q_OBJECT

public:
  About( QWidget * parent, std::vector< sptr< Dictionary::Class > > * dictonaries );

private:

  Ui::About ui;
};

