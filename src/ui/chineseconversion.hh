/* This file is (c) 2015 Zhe Wang <0x1997@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QGroupBox>
#include "config.hh"

namespace Ui {
class ChineseConversion;
}

class ChineseConversion: public QGroupBox
{
  Q_OBJECT

public:
  ChineseConversion( QWidget * parent, Config::Chinese const & );
  ~ChineseConversion();

  void getConfig( Config::Chinese & ) const;

private:
  Ui::ChineseConversion * ui;
};
