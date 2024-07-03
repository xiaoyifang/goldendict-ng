#ifndef __DICTHEADWORDS_H_INCLUDED__
#define __DICTHEADWORDS_H_INCLUDED__

#include <QDialog>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QAction>
#include <qprogressdialog.h>

#include "config.hh"
#include "ui_dictheadwords.h"
#include "dict/dictionary.hh"
#include "headwordsmodel.hh"

class DictHeadwords: public QDialog
{
  Q_OBJECT

public:
  explicit DictHeadwords( QWidget * parent, Config::Class & cfg_, Dictionary::Class * dict_ );
  virtual ~DictHeadwords();

  void setup( Dictionary::Class * dict_ );

protected:
  Config::Class & cfg;
  Dictionary::Class * dict;

  std::shared_ptr< HeadwordListModel > model;
  QSortFilterProxyModel * proxy;
  QString dictId;

  QAction helpAction;

  void saveHeadersToFile();
  bool eventFilter( QObject * obj, QEvent * ev );

private:
  Ui::DictHeadwords ui;
  //  QStringList sortedWords;
  QMutex mutex;
  static void writeWordToFile( QTextStream & out, const QString & word );

private slots:
  void savePos();
  void filterChangedInternal();
  QRegularExpression getFilterRegex() const;
  void filterChanged();
  void exportButtonClicked();
  void okButtonClicked();
  void itemClicked( const QModelIndex & index );
  void autoApplyStateChanged( int state );
  void showHeadwordsNumber();
  void exportAllWords( QProgressDialog & progress, QTextStream & out );
  void loadRegex( QProgressDialog & progress, QTextStream & out );
  virtual void reject();

signals:
  void headwordSelected( QString const &, QString const & );
  void closeDialog();
};

#endif // __DICTHEADWORDS_H_INCLUDED__
