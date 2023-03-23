#ifndef GOLDENDICT_SCANPOPUP_BAR_H
#define GOLDENDICT_SCANPOPUP_BAR_H

#include <QWidget>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>

#include "groupcombobox.hh"
#include "translatebox.hh"
/**
 * The toolbar at the top of the ScanPopup
 */
class ScanPopupBar: public QWidget
{
  Q_OBJECT

 public:
  explicit ScanPopupBar( QWidget * parent = nullptr );
  GroupComboBox * groupList;
  TranslateBox * translateBox;

  QLabel * queryError;
  QToolButton * goBackButton;
  QToolButton * goForwardButton;
  QToolButton * pronounceButton;
  QToolButton * sendWordButton;
  QToolButton * sendWordToFavoritesButton;

  QToolButton * showDictionaryBar;
  QToolButton * onTopButton;
  QToolButton * pinButton;

 private:
  QHBoxLayout * layout;
  QHBoxLayout * h1; // for translateBox and GroupComboBox
  QHBoxLayout * h2; // for other buttons

  void setupIcons() const;
  void retranslateUi() const;
  void setupShortcuts() const;


};


#endif //GOLDENDICT_SCANPOPUP_BAR_H
