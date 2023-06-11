/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "fulltextsearch.hh"
#include "ftshelpers.hh"
#include "gddebug.hh"
#include "help.hh"

#include <QThreadPool>
#include <QMessageBox>
#include <qalgorithms.h>

#if defined( Q_OS_WIN32 )

#include "initializing.hh"
#include <qt_windows.h>
#include <QOperatingSystemVersion>

#endif
#include "globalregex.hh"

namespace FTS
{

void Indexing::run()
{
  try
  {
    timerThread->start();
    // First iteration - dictionaries with no more MaxDictionarySizeForFastSearch articles
    for ( const auto & dictionary : dictionaries ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        break;

      if ( dictionary->canFTS() && !dictionary->haveFTSIndex() ) {
        emit sendNowIndexingName( QString::fromUtf8( dictionary->getName().c_str() ) );
        dictionary->makeFTSIndex( isCancelled, true );
      }
    }

    // Second iteration - all remaining dictionaries
    for ( const auto & dictionary : dictionaries ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
        break;

      if ( dictionary->canFTS() && !dictionary->haveFTSIndex() ) {
        emit sendNowIndexingName( QString::fromUtf8( dictionary->getName().c_str() ) );
        dictionary->makeFTSIndex( isCancelled, false );
      }
    }

    timerThread->quit();
    timerThread->wait();
  }
  catch( std::exception &ex )
  {
    gdWarning( "Exception occurred while full-text search: %s", ex.what() );
  }
  emit sendNowIndexingName( QString() );
}

void Indexing::timeout()
{
  //display all the dictionary name in the following loop ,may result only one dictionary name been seen.
  //as the interval is so small.
  for ( const auto & dictionary : dictionaries ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) )
      break;

    auto newProgress = dictionary->getIndexingFtsProgress();
    if ( newProgress > 0 && newProgress < 100 ) {
      emit sendNowIndexingName(
        QString( "%1......%%2" ).arg( QString::fromStdString( dictionary->getName() ) ).arg( newProgress ) );
    }
  }
}

FtsIndexing::FtsIndexing( std::vector< sptr< Dictionary::Class > > const & dicts):
  dictionaries( dicts ),
  started( false )
{
}

void FtsIndexing::doIndexing()
{
  if( started )
    stopIndexing();

  if( !started )
  {
    while( Utils::AtomicInt::loadAcquire( isCancelled ) )
      isCancelled.deref();

    Indexing *idx = new Indexing( isCancelled, dictionaries, indexingExited );

    connect( idx, &Indexing::sendNowIndexingName, this, &FtsIndexing::setNowIndexedName );

    QThreadPool::globalInstance()->start( idx );

    started = true;
  }
}

void FtsIndexing::stopIndexing()
{
  if( started )
  {
    if( !Utils::AtomicInt::loadAcquire( isCancelled ) )
      isCancelled.ref();

    indexingExited.acquire();
    started = false;

    setNowIndexedName( QString() );
  }
}

void FtsIndexing::setNowIndexedName( const QString & name )
{
  {
    QMutexLocker _( &nameMutex );
    nowIndexing = name;
  }
  emit newIndexingName( name );
}

QString FtsIndexing::nowIndexingName()
{
  QMutexLocker _( &nameMutex );
  return nowIndexing;
}

void addSortedHeadwords( QList< FtsHeadword > & base_list, QList< FtsHeadword > const & add_list)
{
  QList< FtsHeadword > list;

  if( add_list.isEmpty() )
    return;

  if( base_list.isEmpty() )
  {
    base_list = add_list;
    return;
  }

  list.reserve( base_list.size() + add_list.size() );

  QList< FtsHeadword >::iterator base_it = base_list.begin();
  QList< FtsHeadword >::const_iterator add_it = add_list.constBegin();

  while( base_it != base_list.end() || add_it != add_list.end() )
  {
    if( base_it == base_list.end() )
    {
      while( add_it != add_list.end() )
      {
        list.append( *add_it );
        ++add_it;
      }
      break;
    }

    if( add_it == add_list.end() )
    {
      while( base_it != base_list.end() )
      {
        list.append( *base_it );
        ++base_it;
      }
      break;
    }

    if( *add_it < *base_it )
    {
      list.append( *add_it );
      ++add_it;
    }
    else if( *add_it == *base_it )
    {
      base_it->dictIDs.append( add_it->dictIDs );
      for( QStringList::const_iterator itr = add_it->foundHiliteRegExps.constBegin();
             itr != add_it->foundHiliteRegExps.constEnd(); ++itr )
      {
        if( !base_it->foundHiliteRegExps.contains( *itr ) )
          base_it->foundHiliteRegExps.append( *itr );
      }
      ++add_it;
    }
    else
    {
      list.append( *base_it );
      ++base_it;
    }
  }

  base_list.swap( list );
}

FullTextSearchDialog::FullTextSearchDialog( QWidget * parent,
                                            Config::Class & cfg_,
                                            std::vector< sptr< Dictionary::Class > > const & dictionaries_,
                                            std::vector< Instances::Group > const & groups_,
                                            FtsIndexing & ftsidx ):
  QDialog( parent ),
  cfg( cfg_ ),
  dictionaries( dictionaries_ ),
  groups( groups_ ),
  group( 0 ),
  ftsIdx( ftsidx ),
  helpAction( this )
{
  ui.setupUi( this );

  setAttribute( Qt::WA_DeleteOnClose, false );
  setWindowFlags( windowFlags() & ~Qt::WindowContextHelpButtonHint );

  setWindowTitle( tr( "Full-text search" ) );

  if( cfg.preferences.fts.dialogGeometry.size() > 0 )
    restoreGeometry( cfg.preferences.fts.dialogGeometry );

  setNewIndexingName( ftsIdx.nowIndexingName() );

  connect( &ftsIdx, &FtsIndexing::newIndexingName, this, &FullTextSearchDialog::setNewIndexingName );
  connect( GlobalBroadcaster::instance(),
           &GlobalBroadcaster::indexingDictionary,
           this,
           &FullTextSearchDialog::setNewIndexingName );

  ui.searchMode->addItem( tr( "Whole words" ), WholeWords );
  ui.searchMode->addItem( tr( "Plain text"), PlainText );
  ui.searchMode->addItem( tr( "Wildcards" ), Wildcards );

  ui.searchLine->setToolTip(tr("support xapian search syntax,such as AND OR +/- etc"));

  ui.searchMode->setCurrentIndex( cfg.preferences.fts.searchMode );

  ui.searchProgressBar->hide();

  model = new HeadwordsListModel( this, results, activeDicts );
  ui.headwordsView->setModel( model );

  ui.articlesFoundLabel->setText( tr( "Articles found: " ) + "0" );

  connect( ui.headwordsView, &QAbstractItemView::clicked, this, &FullTextSearchDialog::itemClicked );

  connect( this, &QDialog::finished, this, &FullTextSearchDialog::saveData );

  connect( ui.OKButton, &QPushButton::clicked, this, &QDialog::accept );
  connect( ui.cancelButton, &QPushButton::clicked, this, &QDialog::reject );


  helpAction.setShortcut( QKeySequence( "F1" ) );
  helpAction.setShortcutContext( Qt::WidgetWithChildrenShortcut );

  connect( &helpAction, &QAction::triggered, [](){
    Help::openHelpWebpage(Help::section::ui_fulltextserch);
  } );
  connect( ui.helpButton, &QAbstractButton::clicked, &helpAction,&QAction::trigger);

  addAction( &helpAction );

  ui.headwordsView->installEventFilter( this );

  delegate = new WordListItemDelegate( ui.headwordsView->itemDelegate() );
  if( delegate )
    ui.headwordsView->setItemDelegate( delegate );

  ui.searchLine->selectAll();
}

void FullTextSearchDialog::setSearchText( const QString & text )
{
  ui.searchLine->setText( text );
}

FullTextSearchDialog::~FullTextSearchDialog()
{
  if( delegate )
    delegate->deleteLater();
}

void FullTextSearchDialog::stopSearch()
{
  if( !searchReqs.empty() )
  {
    for( std::list< sptr< Dictionary::DataRequest > >::iterator it = searchReqs.begin();
         it != searchReqs.end(); ++it )
      if( !(*it)->isFinished() )
        (*it)->cancel();

    while( searchReqs.size() )
      QApplication::processEvents();
  }
}

void FullTextSearchDialog::showDictNumbers()
{
  ui.totalDicts->setText( QString::number( activeDicts.size() ) );

  unsigned ready = 0, toIndex = 0;
  for( unsigned x = 0; x < activeDicts.size(); x++ )
  {
    if( activeDicts.at( x )->haveFTSIndex() )
      ready++;
    else
      toIndex++;
  }

  ui.readyDicts->setText( QString::number( ready ) );
  ui.toIndexDicts->setText( QString::number( toIndex ) );
}

void FullTextSearchDialog::saveData()
{
  cfg.preferences.fts.searchMode = ui.searchMode->currentIndex();

  cfg.preferences.fts.dialogGeometry = saveGeometry();
}

void FullTextSearchDialog::setNewIndexingName( QString name )
{
  ui.nowIndexingLabel->setText( tr( "Now indexing: " )
                                + ( name.isEmpty() ? tr( "None" ) : name ) );
  showDictNumbers();
}

void FullTextSearchDialog::accept()
{
  const int mode = ui.searchMode->itemData( ui.searchMode->currentIndex() ).toInt();

  model->clear();
  matchedCount=0;
  ui.articlesFoundLabel->setText( tr( "Articles found: " ) + QString::number( results.size() ) );

  if ( ui.searchLine->text().isEmpty() ) {
    QMessageBox message( QMessageBox::Warning,
                         "GoldenDict",
                         tr( "The querying word can not be empty." ),
                         QMessageBox::Ok,
                         this );
    message.exec();
    return;
  }

  if( activeDicts.empty() )
  {
    QMessageBox message( QMessageBox::Warning,
                         "GoldenDict",
                         tr( "No dictionaries for full-text search" ),
                         QMessageBox::Ok,
                         this );
    message.exec();
    return;
  }

  ui.OKButton->setEnabled( false );
  ui.searchProgressBar->show();

  // Make search requests
  for ( unsigned x = 0; x < activeDicts.size(); ++x ) {
    if ( !activeDicts[ x ]->haveFTSIndex() ) {
      continue;
    }
    //max results=100
    sptr< Dictionary::DataRequest > req =
      activeDicts[ x ]->getSearchResults( ui.searchLine->text(), mode, false, false );
    connect( req.get(),
      &Dictionary::Request::finished,
      this,
      &FullTextSearchDialog::searchReqFinished,
      Qt::QueuedConnection );

    connect( req.get(),
      &Dictionary::Request::matchCount,
      this,
      &FullTextSearchDialog::matchCount,
      Qt::QueuedConnection );

    searchReqs.push_back( req );
  }

  searchReqFinished(); // Handle any ones which have already finished
}

void FullTextSearchDialog::searchReqFinished()
{
  QList< FtsHeadword > allHeadwords;
  while ( searchReqs.size() )
  {
    std::list< sptr< Dictionary::DataRequest > >::iterator it;
    for( it = searchReqs.begin(); it != searchReqs.end(); ++it )
    {
      if ( (*it)->isFinished() )
      {
        GD_DPRINTF( "one finished.\n" );

        QString errorString = (*it)->getErrorString();

        if ( (*it)->dataSize() >= 0 || errorString.size() )
        {
          QList< FtsHeadword > * headwords;
          if( (unsigned)(*it)->dataSize() >= sizeof( headwords ) )
          {
            QList< FtsHeadword > hws;
            try
            {
              (*it)->getDataSlice( 0, sizeof( headwords ), &headwords );
              hws.swap( *headwords );
              std::sort( hws.begin(),  hws.end() );
              delete headwords;
              addSortedHeadwords( allHeadwords, hws );
            }
            catch( std::exception & e )
            {
              gdWarning( "getDataSlice error: %s\n", e.what() );
            }
          }

        }
        break;
      }
    }
    if( it != searchReqs.end() )
    {
      GD_DPRINTF( "erasing..\n" );
      searchReqs.erase( it );
      GD_DPRINTF( "erase done..\n" );
      continue;
    }
    else
      break;
  }

  if( !allHeadwords.isEmpty() )
  {
    model->addResults( QModelIndex(), allHeadwords );
    if( results.size() > matchedCount )
      ui.articlesFoundLabel->setText( tr( "Articles found: " ) + QString::number( results.size() ) );
  }

  if ( searchReqs.empty() )
  {
    ui.searchProgressBar->hide();
    ui.OKButton->setEnabled( true );
    QApplication::beep();
  }
}

void FullTextSearchDialog::matchCount(int _matchCount){
  matchedCount+=_matchCount;
  ui.articlesFoundLabel->setText( tr( "Articles found: " )
                                  + QString::number( matchedCount ) );
}

void FullTextSearchDialog::reject()
{
  if( !searchReqs.empty() )
    stopSearch();
  else
  {
    saveData();
    emit closeDialog();
  }
}

void FullTextSearchDialog::itemClicked( const QModelIndex & idx )
{
  if( idx.isValid() && idx.row() < results.size() )
  {
    QString headword = results[ idx.row() ].headword;
    QRegExp reg;
    auto searchText = ui.searchLine->text();
    searchText.replace( RX::Ftx::tokenBoundary, " " );

    auto it = RX::Ftx::token.globalMatch(searchText);
    QString firstAvailbeItem;
    while( it.hasNext() )
    {
      QRegularExpressionMatch match = it.next();

      auto p = match.captured();
      if( p.startsWith( '-' ) )
        continue;

      //the searched text should be like "term".remove enclosed double quotation marks.
      if(p.startsWith("\"")){
        p.remove("\"");
      }

      firstAvailbeItem = p;
      break;
    }

    if( !firstAvailbeItem.isEmpty() )
    {
      reg = QRegExp( firstAvailbeItem, Qt::CaseInsensitive, QRegExp::RegExp2 );
      reg.setMinimal( true );
    }

    emit showTranslationFor( headword, results[ idx.row() ].dictIDs, reg, false );
  }
}

void FullTextSearchDialog::updateDictionaries()
{
  activeDicts.clear();

  // Find the given group

  Instances::Group const * activeGroup = 0;

  for( unsigned x = 0; x < groups.size(); ++x )
    if ( groups[ x ].id == group )
    {
      activeGroup = &groups[ x ];
      break;
    }

  // If we've found a group, use its dictionaries; otherwise, use the global
  // heap.
  std::vector< sptr< Dictionary::Class > > const & groupDicts =
    activeGroup ? activeGroup->dictionaries : dictionaries;

  // Exclude muted dictionaries

  Config::Group const * grp = cfg.getGroup( group );
  Config::MutedDictionaries const * mutedDicts;

  if( group == Instances::Group::AllGroupId )
    mutedDicts = &cfg.mutedDictionaries;
  else
    mutedDicts = grp ? &grp->mutedDictionaries : 0;

  if( mutedDicts && !mutedDicts->isEmpty() )
  {
    activeDicts.reserve( groupDicts.size() );
    for( unsigned x = 0; x < groupDicts.size(); ++x )
      if ( groupDicts[ x ]->canFTS()
           && !mutedDicts->contains( QString::fromStdString( groupDicts[ x ]->getId() ) )
         )
        activeDicts.push_back( groupDicts[ x ] );
  }
  else
  {
    for( unsigned x = 0; x < groupDicts.size(); ++x )
      if ( groupDicts[ x ]->canFTS() )
        activeDicts.push_back( groupDicts[ x ] );
  }

  showDictNumbers();
}

bool FullTextSearchDialog::eventFilter( QObject * obj, QEvent * ev )
{
  if( obj == ui.headwordsView && ev->type() == QEvent::KeyPress )
  {
    QKeyEvent * kev = static_cast< QKeyEvent * >( ev );
    if( kev->key() == Qt::Key_Return || kev->key() == Qt::Key_Enter )
    {
      itemClicked( ui.headwordsView->currentIndex() );
      return true;
    }
  }
  return QDialog::eventFilter( obj, ev );
}

/// HeadwordsListModel

int HeadwordsListModel::rowCount( QModelIndex const & ) const
{
  return headwords.size();
}

QVariant HeadwordsListModel::data( QModelIndex const & index, int role ) const
{
  if( index.row() < 0 )
    return QVariant();

  FtsHeadword const & head = headwords[ index.row() ];

  if ( head.headword.isEmpty() )
    return QVariant();

  switch ( role )
  {
    case Qt::ToolTipRole:
    {
      QString tt;
      for( int x = 0; x < head.dictIDs.size(); x++ )
      {
        if( x != 0 )
          tt += "<br>";

        int n = getDictIndex( head.dictIDs[ x ] );
        if( n != -1 )
          tt += QString::fromUtf8( dictionaries[ n ]->getName().c_str() ) ;
      }
      return tt;
    }

    case Qt::DisplayRole :
      return head.headword;

    case Qt::EditRole :
      return head.headword;

    default:;
  }

  return QVariant();
}

void HeadwordsListModel::addResults(const QModelIndex & parent, QList< FtsHeadword > const & hws )
{
Q_UNUSED( parent );
  beginResetModel();

  addSortedHeadwords( headwords, hws );

  endResetModel();
  emit contentChanged();
}

bool HeadwordsListModel::clear()
{
  beginResetModel();

  headwords.clear();

  endResetModel();

  emit contentChanged();

  return true;
}

int HeadwordsListModel::getDictIndex( QString const & id ) const
{
  std::string dictID( id.toUtf8().data() );
  for( unsigned x = 0; x < dictionaries.size(); x++ )
  {
    if( dictionaries[ x ]->getId().compare( dictID ) == 0 )
      return x;
  }
  return -1;
}

QString FtsHeadword::trimQuotes( QString const & str ) const
{
  QString trimmed( str );

  int n = 0;
  while( str[ n ] == '\"' || str[ n ] == '\'' )
    n++;
  if( n )
    trimmed = trimmed.mid( n );

  while( trimmed.endsWith( '\"' ) || trimmed.endsWith( '\'' ) )
    trimmed.chop( 1 );

  return trimmed;
}

bool FtsHeadword::operator <( FtsHeadword const & other ) const
{
  QString first = trimQuotes( headword );
  QString second = trimQuotes( other.headword );

  int result = first.localeAwareCompare( second );
  if( result )
    return result < 0;

  // Headwords without quotes are equal

  if( first.size() != headword.size() || second.size() != other.headword.size() )
    return headword.localeAwareCompare( other.headword ) < 0;

  return false;
}

} // namespace FTS
