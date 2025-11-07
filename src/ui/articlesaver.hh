/* Small helper to centralize article saving logic shared by MainWindow and ScanPopup */
#pragma once

#include <QWidget>
#include <QObject>
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
// Article saver object that performs the same logic as the old free function.
// It emits `statusMessage` for UI code to present to the user (status bars,
// notifications, etc.). The saver itself is a QObject so it can be used with
// Qt's signal/slot mechanism and can be parented for automatic cleanup.
class ArticleSaver: public QObject
{
	Q_OBJECT

public:
	explicit ArticleSaver( ArticleView * view, QWidget * uiParent, Config::Class & cfg );
	~ArticleSaver() override;

	// Start the save operation. The operation is asynchronous where needed
	// (e.g. resource downloads), but `save()` returns immediately.
	void save();

signals:
	void statusMessage( const QString & text, int timeout );
private:
	ArticleView * view_;
	QWidget * uiParent_;
	Config::Class & cfg_;
};

} // namespace ArticleSaver
