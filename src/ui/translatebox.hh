/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef TRANSLATEBOX_HH
#define TRANSLATEBOX_HH

#include "suggestion.hh"


#include <QLineEdit>
#include <QWidget>
#include <QListWidget>
#include <QFocusEvent>
#include <QCompleter>
#include "wordfinder.hh"


class TranslateBox : public QWidget
{
  Q_OBJECT

public:
  explicit TranslateBox(QWidget * parent = 0);
  QLineEdit * translateLine();
  QWidget * completerWidget();
  Suggestion * wordList();
  void wordList(Suggestion * _word_list);
  void setText(QString text, bool showPopup=true);
  void setSizePolicy(QSizePolicy policy);
  inline void setSizePolicy(QSizePolicy::Policy hor, QSizePolicy::Policy ver)
  { setSizePolicy(QSizePolicy(hor, ver)); }

  void setModel(QStringList & words);

signals:

public slots:
  void setPopupEnabled(bool enable);

private slots:
  void showPopup();
  void rightButtonClicked();
  void onTextEdit();

private:
  Suggestion * word_list;
  QLineEdit * translate_line;
  bool m_popupEnabled;
  QMutex translateBoxMutex;
QCompleter * completer;
  QStringList words;
};

#endif // TRANSLATEBOX_HH
