#pragma once

#include <QLineEdit>
#include <QAction>

class ArticleViewContext
{
public:
  const QLineEdit * translateLine = nullptr;
  QAction * dictionaryBarToggled  = nullptr;
  unsigned currentGroupId         = 0;
  bool popupView                  = false;

  ArticleViewContext() = default;
};
