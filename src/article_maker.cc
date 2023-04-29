/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "article_maker.hh"
#include "config.hh"
#include "folding.hh"
#include "gddebug.hh"
#include "globalbroadcaster.hh"
#include "globalregex.hh"
#include "htmlescape.hh"
#include "langcoder.hh"
#include "utils.hh"
#include "wstring_qt.hh"
#include <QFile>
#include <QTextDocumentFragment>
#include <QUrl>

using std::vector;
using std::string;
using gd::wstring;
using std::set;
using std::list;

inline bool ankiConnectEnabled() { return GlobalBroadcaster::instance()->getPreference()->ankiConnectServer.enabled; }

ArticleMaker::ArticleMaker( vector< sptr< Dictionary::Class > > const & dictionaries_,
                            vector< Instances::Group > const & groups_,
                            const Config::Preferences & cfg_ ):
  dictionaries( dictionaries_ ),
  groups( groups_ ),
  cfg( cfg_ )
{
}


std::string ArticleMaker::makeHtmlHeader( QString const & word,
                                          QString const & icon,
                                          bool expandOptionalParts ) const
{
  string result = R"(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
)";

  // add jquery
  {
    result += R"(<script src="qrc:///scripts/jquery-3.6.0.slim.min.js"></script>)";
    result += R"(<script> var $_$=$.noConflict(); </script>)";
    result += R"(<script src="qrc:///scripts/gd-custom.js"></script>)";
    result += R"(<script src="qrc:///scripts/iframeResizer.min.js"></script>)";
  }

  // add qwebchannel
  {
    result += R"(<script src="qrc:///qtwebchannel/qwebchannel.js"></script>)";
  }

  // document ready ,init webchannel
  {
    result += R"(
    <script>
     $_$(document).ready( function ($){ 
         console.log("webchannel ready..."); 
         new QWebChannel(qt.webChannelTransport, function(channel) { 
             window.articleview = channel.objects.articleview; 
       }); 
     }); 
    </script>
    )";
  }

  // Add a css stylesheet
  {
    result += R"(<link href="qrc:///article-style.css"  media="all" rel="stylesheet" type="text/css">)";

    if ( cfg.displayStyle.size() )
    {
      // Load an additional stylesheet
      QString displayStyleCssFile = QString("qrc:///article-style-st-%1.css").arg(cfg.displayStyle);
      result += "<link href=\"" + displayStyleCssFile.toStdString() +
                R"("  media="all" rel="stylesheet" type="text/css">)";
    }

    result += readCssFile(Config::getUserCssFileName() ,"all");

    if( !cfg.addonStyle.isEmpty() )
    {
      QString name = Config::getStylesDir() + cfg.addonStyle
                     + QDir::separator() + "article-style.css";

      result += readCssFile(name ,"all");
    }

    // Turn on/off expanding of article optional parts
    if( expandOptionalParts )
    {
      result += R"(<!-- Expand optional parts css -->
                   <style type="text/css" media="all">
                    .dsl_opt{
                      display: inline;
                     }
                    .hidden_expand_opt{  display: none;}
                    </style>)";
    }

  }

  // Add print-only css
  {
    result += R"(<link href="qrc:///article-style-print.css"  media="print" rel="stylesheet" type="text/css">)";

    result += readCssFile(Config::getUserCssPrintFileName() ,"print");

    if( !cfg.addonStyle.isEmpty() )
    {
      QString name = Config::getStylesDir() + cfg.addonStyle
                     + QDir::separator() + "article-style-print.css";
      result += readCssFile(name ,"print");
    }
  }

  result += "<title>" + Html::escape( word.toStdString()) + "</title>";

  // This doesn't seem to be much of influence right now, but we'll keep
  // it anyway.
  if ( icon.size() )
    result += R"(<link rel="icon" type="image/png" href="qrc:///flags/)" + Html::escape( icon.toUtf8().data() ) + "\" >\n";

  result += QString::fromUtf8( R"(
<script>
     function tr(key) {
            var tr_map = {
                "Expand article": "%1", "Collapse article": "%2"
            };
            return tr_map[key] || '';
        }
</script>
)" ).arg( tr( "Expand article" ), tr( "Collapse article" ) )
            .toStdString();

  result+= R"(<script src="qrc:///scripts/gd-builtin.js"></script>)";

  if( GlobalBroadcaster::instance()->getPreference()->darkReaderMode )
  {
    result += R"(<link href="qrc:///article-style-darkmode.css"  media="all" rel="stylesheet" type="text/css">)";

    // #242525 because Darkreader will invert pure white to this value
    result += R"(
<script src="qrc:///scripts/darkreader.js"></script>
<style>
body { background: #242525; }
.gdarticle { background: initial;}

.gdarticlebody img{
  background: white;
}
</style>
<script>
  // This function returns a promise, but it is synchroneous because it does not use await
  function fetchShim(src) {
    if (src.startsWith('gdlookup://')) {
      // See https://github.com/xiaoyifang/goldendict/issues/363
      console.error('Dark Reader discovered unexpected URL', src);
      return Promise.resolve({blob: () => new Blob()});
    }
    if (src.startsWith('qrcx://') || src.startsWith('qrc://') || src.startsWith('bres://') || src.startsWith('gico://')) {
      // This is a resource URL, need to fetch and transform
      return new Promise((resolve) => {
        const img = document.createElement('img');
        img.addEventListener('load', () => {
          // Set willReadFrequently to true to tell engine to store data in RAM-backed buffer and not on GPU
          const canvas = document.createElement('canvas', {willReadFrequently: true});
          canvas.width = img.naturalWidth;
          canvas.height = img.naturalHeight;
          const ctx = canvas.getContext('2d');
          ctx.drawImage(img, 0, 0);
          canvas.toBlob((blob) => {
            resolve({blob: () => blob});
          });
        }, false);
        img.src = src;
      });
    }
    // This is a standard URL, can fetch it directly
    return fetch(src);
  }
  DarkReader.setFetchMethod(fetchShim);
  DarkReader.enable({
    brightness: 100,
    contrast: 90,
    sepia: 10
  });
</script>
)";
  }
  result += "</head><body>";

  return result;
}

 std::string ArticleMaker::readCssFile(QString  const & fileName, std::string media)  const{
  QFile addonCss(fileName);
  std::string result;
  if (addonCss.open(QFile::ReadOnly)) {
    QByteArray css = addonCss.readAll();
    if (!css.isEmpty()) {
      result += "<!-- Addon style css -->\n";
      result += R"(<style type="text/css" media=")" + media + "\">\n";
      result += css.data();
      result += "</style>\n";
    }
  }
  return result;
}

std::string ArticleMaker::makeNotFoundBody( QString const & word,
                                            QString const & group )
{
  string result( "<div class=\"gdnotfound\"><p>" );

  QString str( word );
  if( str.isRightToLeft() )
  {
    str.insert( 0, (ushort)0x202E ); // RLE, Right-to-Left Embedding
    str.append( (ushort)0x202C ); // PDF, POP DIRECTIONAL FORMATTING
  }

  if ( word.size() )
    result += tr( "No translation for <b>%1</b> was found in group <b>%2</b>." ).
              arg( QString::fromUtf8( Html::escape( str.toUtf8().data() ).c_str() ), QString::fromUtf8( Html::escape( group.toUtf8().data() ).c_str() ) ).
                toUtf8().data();
  else
    result += tr( "No translation was found in group <b>%1</b>." ).
              arg( QString::fromUtf8( Html::escape( group.toUtf8().data() ).c_str() ) ).
                toUtf8().data();

  result += "</p></div>";

  return result;
}

sptr< Dictionary::DataRequest > ArticleMaker::makeDefinitionFor(
  Config::InputPhrase const & phrase, unsigned groupId,
  QMap< QString, QString > const & contexts,
  QSet< QString > const & mutedDicts,
  QStringList const & dictIDs , bool ignoreDiacritics ) const
{
  if( !dictIDs.isEmpty() )
  {
    QStringList ids = dictIDs;
    std::vector< sptr< Dictionary::Class > > ftsDicts;

    // Find dictionaries by ID's
    for( unsigned x = 0; x < dictionaries.size(); x++ )
    {
      for( QStringList::Iterator it = ids.begin(); it != ids.end(); ++it )
      {
        if( *it == QString::fromStdString( dictionaries[ x ]->getId() ) )
        {
          ftsDicts.push_back( dictionaries[ x ] );
          ids.erase( it );
          break;
        }
      }
      if( ids.isEmpty() )
        break;
    }

    string header = makeHtmlHeader( phrase.phrase, QString(), true );

    return std::make_shared<ArticleRequest>( phrase, "",
                               contexts, ftsDicts, header,
                               -1, true );
  }

  if ( groupId == Instances::Group::HelpGroupId )
  {
    // This is a special group containing internal welcome/help pages
    string result = makeHtmlHeader( phrase.phrase, QString(), cfg.alwaysExpandOptionalParts);

    if ( phrase.phrase == tr( "Welcome!" ) )
    {
      result += tr(
"<h3 align=\"center\">Welcome to <b>GoldenDict</b>!</h3>"
"<p>To start working with the program, first visit <b>Edit|Dictionaries</b> to add some directory paths where to search "
"for the dictionary files, set up various Wikipedia sites or other sources, adjust dictionary order or create dictionary groups."
"<p>And then you're ready to look up your words! You can do that in this window "
"by using a pane to the left, or you can <a href=\"Working with popup\">look up words from other active applications</a>. "
"<p>To customize program, check out the available preferences at <b>Edit|Preferences</b>. "
"All settings there have tooltips, be sure to read them if you are in doubt about anything."
"<p>Should you need further help, have any questions, "
"suggestions or just wonder what the others think, you are welcome at the program's <a href=\"https://github.com/xiaoyifang/goldendict/discussions\">forum</a>."
"<p>Check program's <a href=\"https://github.com/xiaoyifang/goldendict\">website</a> for the updates. "
"<p>(c) 2008-2013 Konstantin Isakov. Licensed under GPLv3 or later."

        ).toUtf8().data();
    }
    else
    if ( phrase.phrase == tr( "Working with popup" ) )
    {
      result += ( tr( "<h3 align=\"center\">Working with the popup</h3>"

"To look up words from other active applications, you would need to first activate the <i>\"Scan popup functionality\"</i> in <b>Preferences</b>, "
"and then enable it at any time either by triggering the 'Popup' icon above, or "
"by clicking the tray icon down below with your right mouse button and choosing so in the menu you've popped. " ) +

#ifdef Q_OS_WIN32
  tr( "Then just stop the cursor over the word you want to look up in another application, "
       "and a window would pop up which would describe it to you." )
#else
  tr( "Then just select any word you want to look up in another application by your mouse "
      "(double-click it or swipe it with mouse with the button pressed), "
      "and a window would pop up which would describe the word to you." )
#endif
  ).toUtf8().data();
    }
    else
    {
      // Not found
      return makeNotFoundTextFor( phrase.phrase, "help" );
    }

    result += "</body></html>";

    sptr< Dictionary::DataRequestInstant > r =  std::make_shared<Dictionary::DataRequestInstant>( true );

    r->appendDataSlice( result.data(), result.size() );
    return r;
  }

  // Find the given group

  Instances::Group const * activeGroup = 0;

  for( unsigned x = 0; x < groups.size(); ++x )
    if ( groups[ x ].id == groupId )
    {
      activeGroup = &groups[ x ];
      break;
    }

  // If we've found a group, use its dictionaries; otherwise, use the global
  // heap.
  std::vector< sptr< Dictionary::Class > > const & activeDicts =
    activeGroup ? activeGroup->dictionaries : dictionaries;

  string header = makeHtmlHeader( phrase.phrase,
                                  activeGroup && activeGroup->icon.size() ?
                                    activeGroup->icon : QString(),
                                  cfg.alwaysExpandOptionalParts );

  if ( mutedDicts.size() )
  {
    std::vector< sptr< Dictionary::Class > > unmutedDicts;

    unmutedDicts.reserve( activeDicts.size() );

    for( unsigned x = 0; x < activeDicts.size(); ++x )
      if ( !mutedDicts.contains(
              QString::fromStdString( activeDicts[ x ]->getId() ) ) )
        unmutedDicts.push_back( activeDicts[ x ] );

    return  std::make_shared<ArticleRequest>( phrase, activeGroup ? activeGroup->name : "",
                               contexts, unmutedDicts, header,
                               cfg.collapseBigArticles ? cfg.articleSizeLimit : -1,
                               cfg.alwaysExpandOptionalParts, ignoreDiacritics );
  }
  else
    return std::make_shared<ArticleRequest>( phrase, activeGroup ? activeGroup->name : "",
                               contexts, activeDicts, header,
                               cfg.collapseBigArticles ? cfg.articleSizeLimit : -1,
                               cfg.alwaysExpandOptionalParts, ignoreDiacritics );
}

sptr< Dictionary::DataRequest > ArticleMaker::makeNotFoundTextFor(
  QString const & word, QString const & group ) const
{
  string result = makeHtmlHeader( word, QString(), true ) + makeNotFoundBody( word, group ) +
    "</body></html>";

  sptr< Dictionary::DataRequestInstant > r = std::make_shared<Dictionary::DataRequestInstant>( true );

  r->appendDataSlice( result.data(), result.size() );
  return r;
}

sptr< Dictionary::DataRequest > ArticleMaker::makeEmptyPage() const
{
  string result = makeHtmlHeader( tr( "(untitled)" ), QString(), true ) +
    "</body></html>";

  sptr< Dictionary::DataRequestInstant > r =
    std::make_shared<Dictionary::DataRequestInstant>( true );

  r->appendDataSlice( result.data(), result.size() );
  return r;
}

sptr< Dictionary::DataRequest > ArticleMaker::makePicturePage( string const & url ) const
{
  string result = makeHtmlHeader( tr( "(picture)" ), QString(), true )
                  + "<a href=\"javascript: if(history.length>2) history.go(-1)\">"
                  + "<img src=\"" + url + "\" /></a>"
                  + "</body></html>";

  sptr< Dictionary::DataRequestInstant > r =
      std::make_shared<Dictionary::DataRequestInstant>( true );

  r->appendDataSlice( result.data(), result.size() );
  return r;
}


bool ArticleMaker::adjustFilePath( QString & fileName )
{
  QFileInfo info( fileName );
  if( !info.isFile() )
  {
    QString dir = Config::getConfigDir();
    dir.chop( 1 );
    info.setFile( dir + fileName);
    if( info.isFile() )
    {
      fileName = info.canonicalFilePath();
      return true;
    }
  }
  return false;
}

//////// ArticleRequest

ArticleRequest::ArticleRequest( Config::InputPhrase const & phrase,
                                QString const & group_,
                                QMap< QString, QString > const & contexts_,
                                vector< sptr< Dictionary::Class > > const & activeDicts_,
                                string const & header,
                                int sizeLimit,
                                bool needExpandOptionalParts_,
                                bool ignoreDiacritics_ ):
  word( phrase.phrase ),
  group( group_ ),
  contexts( contexts_ ),
  activeDicts( activeDicts_ ),
  articleSizeLimit( sizeLimit ),
  needExpandOptionalParts( needExpandOptionalParts_ ),
  ignoreDiacritics( ignoreDiacritics_ )
{
  if ( !phrase.punctuationSuffix.isEmpty() )
    alts.insert( gd::toWString( phrase.phraseWithSuffix() ) );

  // No need to lock dataMutex on construction

  hasAnyData = true;

  appendDataSlice( (void *) header.data(), header.size() );

  //clear founded dicts.
  emit GlobalBroadcaster::instance()->dictionaryClear( ActiveDictIds{word} );

  // Accumulate main forms
  for( unsigned x = 0; x < activeDicts.size(); ++x )
  {
    sptr< Dictionary::WordSearchRequest > s = activeDicts[ x ]->findHeadwordsForSynonym( gd::removeTrailingZero( word ) );

    connect( s.get(), &Dictionary::Request::finished, this, &ArticleRequest::altSearchFinished, Qt::QueuedConnection );

    altSearches.push_back( s );
  }

  altSearchFinished(); // Handle any ones which have already finished
}

void ArticleRequest::altSearchFinished()
{
  if ( altsDone )
    return;

  // Check every request for finishing
  for( list< sptr< Dictionary::WordSearchRequest > >::iterator i =
         altSearches.begin(); i != altSearches.end(); )
  {
    if ( (*i)->isFinished() )
    {
      // This one's finished
      for( size_t count = (*i)->matchesCount(), x = 0; x < count; ++x )
        alts.insert( (**i)[ x ].word );

      altSearches.erase( i++ );
    }
    else
      ++i;
  }

  if ( altSearches.empty() )
  {
#ifdef QT_DEBUG
    qDebug( "alts finished" );
#endif

    // They all've finished! Now we can look up bodies

    altsDone = true; // So any pending signals in queued mode won't mess us up

    vector< wstring > altsVector( alts.begin(), alts.end() );

#ifdef QT_DEBUG
    for( unsigned x = 0; x < altsVector.size(); ++x )
    {
      qDebug() << "Alt:" << QString::fromStdU32String( altsVector[ x ] );
    }
#endif

    wstring wordStd = gd::toWString( word );

    if( activeDicts.size() <= 1 )
      articleSizeLimit = -1; // Don't collapse article if only one dictionary presented

    for( unsigned x = 0; x < activeDicts.size(); ++x )
    {
      try
      {
        sptr< Dictionary::DataRequest > r =
          activeDicts[ x ]->getArticle( wordStd, altsVector,
                                        gd::removeTrailingZero( contexts.value( QString::fromStdString( activeDicts[ x ]->getId() ) ) ),
                                        ignoreDiacritics );

        connect( r.get(), &Dictionary::Request::finished, this, &ArticleRequest::bodyFinished, Qt::QueuedConnection );

        bodyRequests.push_back( r );
      }
      catch( std::exception & e )
      {
        gdWarning( "getArticle request error (%s) in \"%s\"\n",
                   e.what(), activeDicts[ x ]->getName().c_str() );
      }
    }

    bodyFinished(); // Handle any ones which have already finished
  }
}

int ArticleRequest::findEndOfCloseDiv( const QString &str, int pos )
{
  for( ; ; )
  {
    const int n1 = str.indexOf( "</div>", pos );
    if( n1 <= 0 )
      return n1;

    // will there be some custom tags starts with <div but not <div> ,such as <divider>
    const int n2 = str.indexOf( RX::Html::startDivTag, pos );
    if( n2 <= 0 || n2 > n1 )
      return n1 + 6;

    pos = findEndOfCloseDiv( str, n2 + 1 );
    if( pos <= 0 )
      return pos;
  }
}

bool ArticleRequest::isCollapsable( Dictionary::DataRequest & req ,QString const & dictId) {
  if ( GlobalBroadcaster::instance()->collapsedDicts.contains( dictId ) )
    return true;

  bool collapse = false;

  if( articleSizeLimit >= 0 )
  {
    try
    {
      Mutex::Lock _( dataMutex );
      QString text = QString::fromUtf8( req.getFullData().data(), req.getFullData().size() );

      if( !needExpandOptionalParts )
      {
        // Strip DSL optional parts
        for( ; ; )
        {
          const int pos = text.indexOf( "<div class=\"dsl_opt\"" );
          if( pos > 0 )
          {
            const int endPos = findEndOfCloseDiv( text, pos + 1 );
            if( endPos > pos)
              text.remove( pos, endPos - pos );
            else
              break;
          }
          else
            break;
        }
      }

      int size = htmlTextSize( text );
      if( size > articleSizeLimit )
        collapse = true;
    }
    catch(...)
    {
    }
  }
  return collapse;
}

void ArticleRequest::bodyFinished()
{
  if ( bodyDone )
    return;

  GD_DPRINTF( "some body finished" );

  bool wasUpdated = false;

  QStringList dictIds;
  while ( bodyRequests.size() )
  {
    // Since requests should go in order, check the first one first
    if ( bodyRequests.front()->isFinished() )
    {
      // Good

      GD_DPRINTF( "one finished." );

      Dictionary::DataRequest & req = *bodyRequests.front();

      QString errorString = req.getErrorString();

      if ( req.dataSize() >= 0 || errorString.size() )
      {
        sptr< Dictionary::Class > const & activeDict =
            activeDicts[ activeDicts.size() - bodyRequests.size() ];

        string dictId = activeDict->getId();
        dictIds << QString::fromStdString(dictId);
        string head;

        string gdFrom = "gdfrom-" + Html::escape( dictId );

        if ( closePrevSpan )
        {
          head += R"(</div></div><div style="clear:both;"></div><span class="gdarticleseparator"></span>)";
        }

        bool collapse = isCollapsable( req, QString::fromStdString( dictId ) );

        string jsVal = Html::escapeForJavaScript( dictId );

        head += QString::fromUtf8(
                  R"( <div class="gdarticle %1 %2" id="%3" 
                    onClick="gdMakeArticleActive( '%4' );" 
                    onContextMenu="gdMakeArticleActive( '%4' );">)" )
                  .arg(  closePrevSpan ? "" : " gdactivearticle" ,
                         collapse ? " gdcollapsedarticle" : "" ,
                         gdFrom.c_str() ,
                         jsVal.c_str() )
                  .toStdString();

        closePrevSpan = true;

        head += QString::fromUtf8(
                  R"(<div class="gddictname" onclick="gdExpandArticle('%1');"  %2  id="gddictname-%1" title="%3">
                      <span class="gddicticon"><img src="gico://%1/dicticon.png"></span>
                      <span class="gdfromprefix">%4</span>
                      <span class="gddicttitle">%5</span>
                      <span class="collapse_expand_area"><img class="%6" id="expandicon-%1" title="%7" ></span>
                     </div>)" )
                  .arg( dictId.c_str(),
                        collapse ? R"(style="cursor:pointer;")" : "",
                        collapse ? tr( "Expand article" ) : QString(),
                        Html::escape( tr( "From " ).toStdString() ).c_str(),
                        Html::escape( activeDict->getName() ).c_str(),
                        collapse ? "gdexpandicon" : "gdcollapseicon",
                        collapse ? "" : tr( "Collapse article" )

                          )
                  .toStdString();

        head += R"(<div class="gddictnamebodyseparator"></div>)";

        // If the user has enabled Anki integration in settings,
        // Show a (+) button that lets the user add a new Anki card.
        if ( ankiConnectEnabled() ) {
          QString link{ R"EOF(
          <a href="ankicard:%1" class="ankibutton" title="%2" >
          <img src="qrc:///icons/add-anki-icon.svg">
          </a>
          )EOF" };
          head += link.arg( Html::escape( dictId ).c_str(), tr( "Make a new Anki note" ) ).toStdString();
        }

        head += QString::fromUtf8(
                  R"(<div class="gdarticlebody gdlangfrom-%1" lang="%2" style="display:%3" id="gdarticlefrom-%4">)" )
                  .arg( LangCoder::intToCode2( activeDict->getLangFrom() ),
                        LangCoder::intToCode2( activeDict->getLangTo() ),
                        collapse ? "none" : "inline",
                        dictId.c_str()  )
                  .toStdString();

        if( errorString.size() ) {
          head += "<div class=\"gderrordesc\">"
            + Html::escape( tr( "Query error: %1" ).arg( errorString ).toUtf8().data() ) + "</div>";
        }

        appendDataSlice( head.data(), head.size() );

        try {
          if( req.dataSize() > 0 ) {
            auto d = bodyRequests.front()->getFullData();
            appendDataSlice( &d.front(), d.size() );
          }
        }
        catch( std::exception & e ) {
          gdWarning( "getDataSlice error: %s\n", e.what() );
        }

        wasUpdated = true;

        foundAnyDefinitions = true;
      }
      GD_DPRINTF( "erasing.." );
      bodyRequests.pop_front();
      GD_DPRINTF( "erase done.." );
    }
    else
    {
      GD_DPRINTF( "one not finished." );
      break;
    }
  }


  if ( bodyRequests.empty() )
  {
    // No requests left, end the article

    bodyDone = true;

    {
      string footer;

      if ( closePrevSpan )
      {
        footer += "</div></div>";
        closePrevSpan = false;
      }

      if ( !foundAnyDefinitions )
      {
        // No definitions were ever found, say so to the user.

        // Larger words are usually whole sentences - don't clutter the output
        // with their full bodies.
        footer += ArticleMaker::makeNotFoundBody( word.size() < 40 ? word : "", group );

        // When there were no definitions, we run stemmed search.
        stemmedWordFinder = std::make_shared< WordFinder >( this );

        connect( stemmedWordFinder.get(),
                 &WordFinder::finished,
                 this,
                 &ArticleRequest::stemmedSearchFinished,
                 Qt::QueuedConnection );

        stemmedWordFinder->stemmedMatch( word, activeDicts );
      }
      else {
        footer += R"(<div class="empty-space"></div>)";

        footer += "</body></html>";
      }

      appendDataSlice( footer.data(), footer.size() );
    }

    if( stemmedWordFinder.get() ) {
      update();
      qDebug() << "send dicts(stemmed):" << word << ":" << dictIds;
      emit GlobalBroadcaster::instance()->dictionaryChanges( ActiveDictIds{ word, dictIds } );
      dictIds.clear();
    }
    else {
      finish();
      qDebug() << "send dicts(finished):" << word << ":" << dictIds;
      emit GlobalBroadcaster::instance()->dictionaryChanges( ActiveDictIds{ word, dictIds } );
      dictIds.clear();
    }
  }
  else if( wasUpdated ) {
    update();
    qDebug() << "send dicts(updated):" << word << ":" << dictIds;
    emit GlobalBroadcaster::instance()->dictionaryChanges( ActiveDictIds{ word, dictIds } );
    dictIds.clear();
  }
}

int ArticleRequest::htmlTextSize( QString html )
{
  // website dictionary.
  if( html.contains( QRegularExpression( "<iframe\\s*[^>]*>", QRegularExpression::CaseInsensitiveOption ) ) )
  {
    //arbitary number;
    return 1000;
  }

  //https://bugreports.qt.io/browse/QTBUG-102757
  QString stripStyleSheet =
    html.remove( QRegularExpression( "<link\\s*[^>]*>", QRegularExpression::CaseInsensitiveOption ) )
      .remove( QRegularExpression( R"(<script[\s\S]*?>[\s\S]*?<\/script>)", QRegularExpression::CaseInsensitiveOption|QRegularExpression::MultilineOption ) );
  int size = QTextDocumentFragment::fromHtml( stripStyleSheet ).toPlainText().length();

  return size;
}

void ArticleRequest::stemmedSearchFinished()
{
  // Got stemmed matching results

  WordFinder::SearchResults sr = stemmedWordFinder->getResults();

  string footer;

  bool continueMatching = false;

  if ( sr.size() )
  {
    footer += R"(<div class="gdstemmedsuggestion"><span class="gdstemmedsuggestion_head">)" +
      Html::escape( tr( "Close words: " ).toUtf8().data() ) +
      "</span><span class=\"gdstemmedsuggestion_body\">";

    for( unsigned x = 0; x < sr.size(); ++x )
    {
      footer += linkWord( sr[ x ].first );

      if ( x != sr.size() - 1 )
      {
        footer += ", ";
      }
    }

    footer += "</span></div>";
  }

  splittedWords = splitIntoWords( word );

  if ( splittedWords.first.size() > 1 ) // Contains more than one word
  {
    disconnect( stemmedWordFinder.get(), &WordFinder::finished, this, &ArticleRequest::stemmedSearchFinished );

    connect( stemmedWordFinder.get(),
      &WordFinder::finished,
      this,
      &ArticleRequest::individualWordFinished,
      Qt::QueuedConnection );

    currentSplittedWordStart = -1;
    currentSplittedWordEnd = currentSplittedWordStart;

    firstCompoundWasFound = false;

    compoundSearchNextStep( false );

    continueMatching = true;
  }

  if ( !continueMatching )
    footer += "</body></html>";

  {
    appendDataSlice( footer.data(), footer.size() );
  }

  if( continueMatching )
    update();
  else
    finish();
}

void ArticleRequest::compoundSearchNextStep( bool lastSearchSucceeded )
{
  if ( !lastSearchSucceeded )
  {
    // Last search was unsuccessful. First, emit what we had.

    string footer;

    if ( lastGoodCompoundResult.size() ) // We have something to append
    {
//      GD_DPRINTF( "Appending\n" );

      if ( !firstCompoundWasFound )
      {
        // Append the beginning
        footer += R"(<div class="gdstemmedsuggestion"><span class="gdstemmedsuggestion_head">)" +
          Html::escape( tr( "Compound expressions: " ).toUtf8().data() ) +
          "</span><span class=\"gdstemmedsuggestion_body\">";

        firstCompoundWasFound = true;
      }
      else
      {
        // Append the separator
        footer += " / ";
      }

      footer += linkWord( lastGoodCompoundResult );

      lastGoodCompoundResult.clear();
    }

    // Then, start a new search for the next word, if possible

    if ( currentSplittedWordStart >= splittedWords.first.size() - 2 )
    {
      // The last word was the last possible to start from

      if ( firstCompoundWasFound )
        footer += "</span>";

      // Now add links to all the individual words. They conclude the result.

      footer += R"(<div class="gdstemmedsuggestion"><span class="gdstemmedsuggestion_head">)" +
        Html::escape( tr( "Individual words: " ).toUtf8().data() ) +
        "</span><span class=\"gdstemmedsuggestion_body\"";
      if( splittedWords.first[ 0 ].isRightToLeft() )
        footer += " dir=\"rtl\"";
      footer += ">";

      footer += escapeSpacing( splittedWords.second[ 0 ] );

      for( int x = 0; x < splittedWords.first.size(); ++x )
      {
        footer += linkWord( splittedWords.first[ x ] );
        footer += escapeSpacing( splittedWords.second[ x + 1 ] );
      }

      footer += "</span>";

      footer += "</body></html>";

      appendToData( footer );

      finish();

      return;
    }

    if ( footer.size() )
    {
      appendToData( footer );
      update();
    }

    // Advance to the next word and start from looking up two words
    ++currentSplittedWordStart;
    currentSplittedWordEnd = currentSplittedWordStart + 1;
  }
  else
  {
    // Last lookup succeeded -- see if we can try the larger sequence

    if ( currentSplittedWordEnd < splittedWords.first.size() - 1 )
    {
      // We can, indeed.
      ++currentSplittedWordEnd;
    }
    else
    {
      // We can't. Emit what we have and start over.

      ++currentSplittedWordEnd; // So we could use the same code for result
                                // emitting

      // Initiate new lookup
      compoundSearchNextStep( false );

      return;
    }
  }

  // Build the compound sequence

  currentSplittedWordCompound = makeSplittedWordCompound();

  // Look it up

//  GD_DPRINTF( "Looking up %s\n", qPrintable( currentSplittedWordCompound ) );

  stemmedWordFinder->expressionMatch( currentSplittedWordCompound, activeDicts, 40, // Would one be enough? Leave 40 to be safe.
                                      Dictionary::SuitableForCompoundSearching );
}

QString ArticleRequest::makeSplittedWordCompound()
{
  QString result;

  result.clear();

  for( int x = currentSplittedWordStart; x <= currentSplittedWordEnd; ++x )
  {
    result.append( splittedWords.first[ x ] );

    if ( x < currentSplittedWordEnd )
    {
      result.append( splittedWords.second[ x + 1 ].simplified() );
    }
  }

  return result;
}

void ArticleRequest::individualWordFinished()
{
  WordFinder::SearchResults const & results = stemmedWordFinder->getResults();

  if ( results.size() )
  {
    wstring source = Folding::applySimpleCaseOnly(currentSplittedWordCompound);

    bool hadSomething = false;

    for( unsigned x = 0; x < results.size(); ++x )
    {
      if ( results[ x ].second )
      {
        // Spelling suggestion match found. No need to continue.
        hadSomething = true;
        lastGoodCompoundResult = currentSplittedWordCompound;
        break;
      }

      // Prefix match found. Check if the aliases are acceptable.

      wstring result( Folding::applySimpleCaseOnly( results[ x ].first ) );

      if ( source.size() <= result.size() && result.compare( 0, source.size(), source ) == 0 )
      {
        // The resulting string begins with the source one

        hadSomething = true;

        if ( source.size() == result.size() )
        {
          // Got the match. No need to continue.
          lastGoodCompoundResult = currentSplittedWordCompound;
          break;
        }
      }
    }

    if ( hadSomething )
    {
      compoundSearchNextStep( true );
      return;
    }
  }

  compoundSearchNextStep( false );
}

void ArticleRequest::appendToData( std::string const & str )
{
  appendDataSlice( str.data(), str.size() );
}

QPair< ArticleRequest::Words, ArticleRequest::Spacings > ArticleRequest::splitIntoWords( QString const & input )
{
  QPair< Words, Spacings > result;

  QChar const * ptr = input.data();

  for( ; ; )
  {
    QString spacing;

    for( ; ptr->unicode() && ( Folding::isPunct( ptr->unicode() ) || Folding::isWhitespace( ptr->unicode() ) ); ++ptr )
      spacing.append( *ptr );

    result.second.append( spacing );

    QString word;

    for( ; ptr->unicode() && !( Folding::isPunct( ptr->unicode() ) || Folding::isWhitespace( ptr->unicode() ) ); ++ptr )
      word.append( *ptr );

    if ( word.isEmpty() )
      break;

    result.first.append( word );
  }

  return result;
}

string ArticleRequest::linkWord( QString const & str )
{
  QUrl url;

  url.setScheme( "gdlookup" );
  url.setHost( "localhost" );
  url.setPath( Utils::Url::ensureLeadingSlash( str ) );

  string escapedResult = Html::escape( str.toUtf8().data() );
  return string( "<a href=\"" ) + url.toEncoded().data() + "\">" + escapedResult +"</a>";
}

std::string ArticleRequest::escapeSpacing( QString const & str )
{
  QByteArray spacing = Html::escape( str.toUtf8().data() ).c_str();

  spacing.replace( "\n", "<br>" );

  return spacing.data();
}

void ArticleRequest::cancel()
{
    if( isFinished() )
        return;
    if( !altSearches.empty() )
    {
        for( list< sptr< Dictionary::WordSearchRequest > >::iterator i =
               altSearches.begin(); i != altSearches.end(); ++i )
        {
            (*i)->cancel();
        }
    }
    if( !bodyRequests.empty() )
    {
        for( list< sptr< Dictionary::DataRequest > >::iterator i =
               bodyRequests.begin(); i != bodyRequests.end(); ++i )
        {
            (*i)->cancel();
        }
    }
    if( stemmedWordFinder.get() ) stemmedWordFinder->cancel();
    finish();
}
