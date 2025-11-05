/* Small helper to centralize article saving logic shared by MainWindow and ScanPopup */
#pragma once

#include <QWidget>
#include <QProgressDialog>

class QWidget;

/// Shared small progress dialog used when saving articles (used by both
/// main window and popup). Kept here to avoid duplicating the same class in
/// multiple headers.
class ArticleSaveProgressDialog: public QProgressDialog
{
  Q_OBJECT

public:
  explicit ArticleSaveProgressDialog( QWidget * parent = nullptr, Qt::WindowFlags f = Qt::Widget ):
    QProgressDialog( parent, f )
  {
    setAutoReset( false );
    setAutoClose( false );
  }

public slots:
  void perform()
  {
    int progress = value() + 1;
    if ( progress == maximum() ) {
      emit close();
      deleteLater();
    }
    setValue( progress );
  }
};


class ArticleView;
namespace Config {
class Class;
}
class QWidget;

namespace ArticleSaver {
// Save the article shown in `view` using UI parent `parent`.
// If `statusBar` is provided, completion/error messages are shown there.
// This function mirrors the existing behavior in `MainWindow::on_saveArticle_triggered`
// and `ScanPopup::saveArticleButton_clicked()` and is intentionally synchronous
// in its API while performing necessary asynchronous operations internally.
// `statusWidget` may be a pointer to `QStatusBar` (main window) or to
// `MainStatusBar` (popup). Pass nullptr to skip status updates.
void saveArticle( ArticleView * view, QWidget * parent, Config::Class & cfg, QWidget * statusWidget = nullptr );
} // namespace ArticleSaver
