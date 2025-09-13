/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QAction>
#include <QMap>
#include <QSet>
#include <list>
#include "article_netmgr.hh"
#include "audio/audioplayerinterface.hh"
#include "instances.hh"
#include "groupcombobox.hh"
#include "globalbroadcaster.hh"
#include "article_inspect.hh"
#include "ankiconnector.hh"
#include "webmultimediadownload.hh"
#include "base_type.hh"
#include "articlewebview.hh"
#include "ui/searchpanel.hh"
#include "ui/ftssearchpanel.hh"
#include "dictionary_group.hh"

class ResourceToSaveHandler;
class ArticleViewAgent;

/// A widget with the web view tailored to view and handle articles -- it
/// uses the appropriate netmgr, handles link clicks, rmb clicks etc
class ArticleView: public QWidget
{
  Q_OBJECT

  ArticleNetworkAccessManager & articleNetMgr;
  const AudioPlayerPtr & audioPlayer;
  std::unique_ptr< DictionaryGroup > dictionaryGroup;
  bool popupView;
  const Config::Class & cfg;
  QWebChannel * channel;
  ArticleViewAgent * agent;

  AnkiConnector * ankiConnector;

  QAction pasteAction, articleUpAction, articleDownAction, goBackAction, goForwardAction, selectCurrentArticleAction,
    copyAsTextAction, inspectAction;

  bool expandOptionalParts;

  /// An action used to create Anki notes.
  QAction sendToAnkiAction{ tr( "&Create Anki note" ), this };

  /// For resources opened via desktop services
  QSet< QString > desktopOpenedTempFiles;

  QAction * dictionaryBarToggled;

  unsigned currentGroupId;
  const QLineEdit * translateLine;

  /// current searching word.
  QString currentWord;

  /// current active dict id list;
  QStringList currentActiveDictIds;

  bool historyMode = false;

  //current active dictionary id;
  QString activeDictId;

  QString audioLink_;

  /// Search in results of full-text search
  QString firstAvailableText;
  QStringList uniqueMatches;

  QString delayedHighlightText;

  /// Flag indicating if this view is dedicated for website browsing
  bool isWebsiteView = false;

  /// Host part of the website URL for identification
  QString websiteHost;

  void highlightFTSResults();
  void performFtsFindOperation( bool backwards );

public:
  /// The popupView flag influences contents of the context menus to be
  /// appropriate to the context of the view.
  /// The groups aren't copied -- rather than that, the reference is kept
  ArticleView( QWidget * parent,
               ArticleNetworkAccessManager &,
               const AudioPlayerPtr &,
               const std::vector< sptr< Dictionary::Class > > & allDictionaries,
               const Instances::Groups &,
               bool popupView,
               const Config::Class & cfg,
               const QLineEdit * translateLine,
               QAction * dictionaryBarToggled = nullptr,
               unsigned currentGroupId        = 0 );

  void setCurrentGroupId( unsigned currengGrgId );
  unsigned getCurrentGroupId();

  void setAudioLink( QString audioLink );
  QString getAudioLink() const;

  QSize minimumSizeHint() const override;
  void clearContent();

  ~ArticleView();

  void load( QString url );
  /// Returns "gdfrom-" + dictionaryId.
  static QString scrollToFromDictionaryId( const QString & dictionaryId );

  /// Shows the definition of the given word with the given group.
  /// scrollTo can be optionally set to a "gdfrom-xxxx" identifier to position
  /// the page to that article on load.
  /// contexts is an optional map of context values to be passed for dictionaries.
  /// The only values to pass here are ones obtained from showDefinitionInNewTab()
  /// signal or none at all.
  void showDefinition( const QString & word,
                       unsigned group,
                       const QString & scrollTo  = QString(),
                       const Contexts & contexts = Contexts() );

  void showDefinition( const QString & word,
                       const QStringList & dictIDs,
                       const QRegularExpression & searchRegExp,
                       unsigned group,
                       bool ignoreDiacritics );
  void showDefinition( const QString & word, const QStringList & dictIDs, unsigned group, bool ignoreDiacritics );

  void sendToAnki( const QString & word, const QString & text, const QString & sentence );
  /// Clears the view and sets the application-global waiting cursor,
  /// which will be restored when some article loads eventually.
  void showAnticipation();

  /// Create a new Anki card from a currently displayed article with the provided id.
  /// This function will call QWebEnginePage::runJavaScript() to fetch the corresponding HTML.
  void makeAnkiCardFromArticle( const QString & article_id );

  /// Opens the given link. Supposed to be used in response to
  /// openLinkInNewTab() signal. The link scheme is therefore supposed to be
  /// one of the internal ones.
  /// contexts is an optional map of context values to be passed for dictionaries.
  /// The only values to pass here are ones obtained from showDefinitionInNewTab()
  /// signal or none at all.
  void openLink( const QUrl & url,
                 const QUrl & referrer,
                 const QString & scrollTo  = QString(),
                 const Contexts & contexts = Contexts() );
  void playAudio( const QUrl & url );
  void audioDownloadFinished( const sptr< Dictionary::DataRequest > & req );

  /// Called when the state of dictionary bar changes and the view is active.
  /// The function reloads content if the change affects it.
  void updateMutedContents();

  bool canGoBack();
  bool canGoForward();

  /// Called when preference changes
  void setSelectionBySingleClick( bool set );

  void setDelayedHighlightText( const QString & text );

  /// \brief Set background as black if darkreader mode is enabled.
  void syncBackgroundColorWithCfgDarkReader() const;

  QString getCurrentWord();


private:
  // widgets
  ArticleWebView * webview;
  SearchPanel * searchPanel;
  FtsSearchPanel * ftsSearchPanel;

public slots:

  /// Goes back in history
  void back();

  /// Goes forward in history
  void forward();

  /// Takes the focus to the view
  void focus()
  {
    webview->setFocus();
  }

  /// Sends *word* to Anki.
  void handleAnkiAction();

public:

  /// Reloads the view
  void reload();

  void stopSound();

  /// Returns true if there's an audio reference on the page, false otherwise.
  void hasSound( const std::function< void( bool has ) > & callback );

  /// Plays the first audio reference on the page, if any.
  void playSound();

  void setZoomFactor( qreal factor )
  {
    qreal existedFactor = webview->zoomFactor();
    if ( !qFuzzyCompare( existedFactor, factor ) ) {
      qDebug() << "zoom factor ,existed:" << existedFactor << "set:" << factor;
      webview->setZoomFactor( factor );
      //webview->page()->setZoomFactor(factor);
    }
  }

  /// Returns whether this view is for website browsing
  bool isWebsite() const
  {
    return isWebsiteView;
  }

  /// Sets whether this view is for website browsing
  void setWebsite(bool website) { isWebsiteView = website; }

  /// Returns the website host
  QString getWebsiteHost() const
  {
    return websiteHost;
  }

  /// Sets website host
  void setWebsiteHost( const QString & host );

  /// Returns current article's text in .html format
  void toHtml( const std::function< void( QString & ) > & callback );

  void setHtml( const QString & content, const QUrl & baseUrl );
  QWebEnginePage * page();
  void setContent( const QByteArray & data, const QString & mimeType = QString(), const QUrl & baseUrl = QUrl() );

  /// Returns current article's title
  QString getTitle();

  /// Returns the phrase translated by the current article.
  QString getWord() const;

  /// Prints current article
  void print( QPrinter * ) const;

  /// Closes search if it's open and returns true. Returns false if it
  /// wasn't open.
  bool closeSearch();

  bool isSearchOpened();

  /// Jumps to the article specified by the dictionary id,
  /// by executing a javascript code.
  void jumpToDictionary( const QString &, bool force );

  /// Returns all articles currently present in view, as a list of dictionary
  /// string ids.
  QStringList getArticlesList();

  /// Returns the dictionary id of the currently active article in the view.
  QString getActiveArticleId();
  void setActiveArticleId( const QString & );

  ResourceToSaveHandler * saveResource( const QUrl & url, const QString & fileName );

  void findText( QString & text,
                 const QWebEnginePage::FindFlags & f,
                 const std::function< void( bool match ) > & callback = nullptr );

signals:

  void titleChanged( ArticleView *, const QString & title );

  void pageLoaded( ArticleView * );

  /// Signals that the following link was requested to be opened in new tab
  void openLinkInNewTab( const QUrl &, const QUrl & referrer, const QString & fromArticle, const Contexts & contexts );
  /// Signals that the following definition was requested to be showed in new tab
  void showDefinitionInNewTab( const QString & word,
                               unsigned group,
                               const QString & fromArticle,
                               const Contexts & contexts );

  /// Put translated word into history
  void sendWordToHistory( const QString & word );

  /// Emitted when user types a text key. This should typically be used to
  /// switch focus to word input.
  void typingEvent( const QString & text );

  void statusBarMessage( const QString & message, int timeout = 0, const QPixmap & pixmap = QPixmap() );

  /// Signals that the dictionaries pane was requested to be showed
  void showDictsPane();
  /// Signals that the founded dictionaries ready to be showed
  void updateFoundInDictsList();

  /// Emitted when an article becomes active,
  /// typically in response to user actions
  /// (clicking on the article or using shortcuts).
  /// id - the dictionary id of the active article.
  void activeArticleChanged( const ArticleView *, const QString & id );

  /// Signal to add word to history even if history is disabled
  void forceAddWordToHistory( const QString & word );

  /// Signal to close popup menu
  void closePopupMenu();

  void sendWordToInputLine( const QString & word );

  void storeResourceSavePath( const QString & );

  void zoomIn();
  void zoomOut();

  ///  signal finished javascript;
  void notifyJavascriptFinished();

  void inspectSignal( QWebEnginePage * page );

  void saveBookmarkSignal( const QString & bookmark );

public slots:

  /// Opens the search (Ctrl+F)
  void openSearch();

  void on_searchPrevious_clicked();
  void on_searchNext_clicked();

  void onJsActiveArticleChanged( const QString & id );

  /// Handles F3 and Shift+F3 for search navigation
  bool handleF3( QObject * obj, QEvent * ev );

  /// Selects an entire text of the current article
  void selectCurrentArticle();
  //receive signal from weburlinterceptor.
  void linkClicked( const QUrl & );
  //aim to receive signal from html. the fragment url click to  navigation through page wil not be intecepted by weburlinteceptor
  Q_INVOKABLE void linkClickedInHtml( const QUrl & );
private slots:
  void inspectElement();
  void loadFinished( bool ok );
  void handleTitleChanged( const QString & title );
  void attachWebChannelToHtml();

  void linkHovered( const QString & link );
  void contextMenuRequested( const QPoint & );

  bool isAudioLink( QUrl & targetUrl )
  {
    return ( targetUrl.scheme() == "gdau" || Utils::Url::isAudioUrl( targetUrl ) );
  }

  void resourceDownloadFinished( const sptr< Dictionary::DataRequest > & req, const QUrl & resourceDownloadUrl );

  /// We handle pasting by attempting to define the word in clipboard.
  void pasteTriggered();

  unsigned getCurrentGroup();

  /// Nagivates to the previous article relative to the active one.
  void moveOneArticleUp();

  /// Nagivates to the next article relative to the active one.
  void moveOneArticleDown();

  void on_searchText_textEdited();
  void on_searchText_returnPressed();
  void on_searchCloseButton_clicked();
  void on_searchCaseSensitive_clicked( bool );

  void on_ftsSearchPrevious_clicked();
  void on_ftsSearchNext_clicked();

  /// Handles the double-click from the definition.
  void doubleClicked( QPoint pos );

  /// Handles audio player error message
  void audioPlayerError( const QString & message );

  /// Copy current selection as plain text
  void copyAsText();

  void setActiveDictIds( const ActiveDictIds & ad );

  void dictionaryClear( const ActiveDictIds & ad );

private:

  /// Deduces group from the url. If there doesn't seem to be any group,
  /// returns 0.
  unsigned getGroup( const QUrl & );

  /// Returns current article in the view, in the form of "gdfrom-xxx" id.
  QString getCurrentArticle();

  /// Sets the current article by executing a javascript code.
  /// If moveToIt is true, it moves the focus to it as well.
  /// Returns true in case of success, false otherwise.
  bool setCurrentArticle( const QString &, bool moveToIt = false );

  /// Checks if the given article in form of "gdfrom-xxx" is inside a "website"
  /// frame.
  void isFramedArticle( const QString & article, const std::function< void( bool framed ) > & callback );

  /// Sees if the last clicked link is from a website frame. If so, changes url
  /// to point to url text translation instead, and saves the original
  /// url to the appropriate "contexts" entry.
  void tryMangleWebsiteClickedUrl( QUrl & url, Contexts & contexts );

  /// Loads a page at @p url into view.
  void load( const QUrl & url );

  /// Attempts removing last temporary file created.
  void cleanupTemp();

  bool eventFilter( QObject * obj, QEvent * ev ) override;

  void performFindOperation( bool backwards );

  /// Returns the comma-separated list of dictionary ids which should be muted
  /// for the given group. If there are none, returns empty string.
  QString getMutedForGroup( unsigned group );

  QStringList getMutedDictionaries( unsigned group );
};

class ResourceToSaveHandler: public QObject
{
  Q_OBJECT

public:
  explicit ResourceToSaveHandler( ArticleView * view, QString fileName );
  void addRequest( const sptr< Dictionary::DataRequest > & req );
  bool isEmpty()
  {
    return downloadRequests.empty();
  }

signals:
  void done();
  void statusBarMessage( const QString & message, int timeout = 0, const QPixmap & pixmap = QPixmap() );

public slots:
  void downloadFinished();

private:
  std::list< sptr< Dictionary::DataRequest > > downloadRequests;
  QString fileName;
  bool alreadyDone;
};

class ArticleViewAgent: public QObject
{
  Q_OBJECT
  ArticleView * articleView;

public:
  explicit ArticleViewAgent( ArticleView * articleView );


public slots:
  Q_INVOKABLE void onJsActiveArticleChanged( const QString & id );
  Q_INVOKABLE void linkClickedInHtml( const QUrl & );
  Q_INVOKABLE void collapseInHtml( const QString & dictId, bool on = true ) const;
};
