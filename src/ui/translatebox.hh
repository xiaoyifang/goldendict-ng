/* This file is (c) 2012 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef TRANSLATEBOX_HH
#define TRANSLATEBOX_HH

#include <QMutex>
#include <QLineEdit>
#include <QWidget>
#include <QListWidget>
#include <QFocusEvent>
#include <QCompleter>

class TranslateBox : public QWidget
{
  Q_OBJECT

public:
  explicit TranslateBox(QWidget * parent = 0);
  QLineEdit * translateLine();
  QWidget * completerWidget();
  void setText(QString text, bool showPopup=true);
  void setSizePolicy(QSizePolicy policy);
  inline void setSizePolicy(QSizePolicy::Policy hor, QSizePolicy::Policy ver)
  { setSizePolicy(QSizePolicy(hor, ver)); }

  void setModel( QStringList & words );

signals:

public slots:
  void setPopupEnabled(bool enable);

private slots:
  void showPopup();
  void rightButtonClicked();
  void onTextEdit();

private:
  QLineEdit * translate_line;
  bool m_popupEnabled;
  QMutex translateBoxMutex;
  QCompleter * completer;
  QStringList words;
};

#endif // TRANSLATEBOX_HH
