/* This file is (c) 2014 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "ftshelpers.hh"
#include "fulltextsearch.hh"
#include "globalregex.hh"
#include "help.hh"
#include <QFutureSynchronizer>
#include <QMessageBox>
#include <QThreadPool>

namespace FTS {

void Indexing::run()
{
  try {
    timerThread->start();
    const int parallel_count = GlobalBroadcaster::instance()->getPreference()->fts.parallelThreads;
    QSemaphore sem( parallel_count < 1 ? 1 : parallel_count );

    QFutureSynchronizer< void > synchronizer;
    qDebug() << "starting create the fts with thread:" << parallel_count;
    for ( const auto & dictionary : dictionaries ) {
      if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
        // synchronizer.setCancelOnWait( true );
        break;
      }

      if ( dictionary->canFTS() && !dictionary->haveFTSIndex() ) {
        sem.acquire();
        const QFuture< void > f = QtConcurrent::run( [ this, &sem, &dictionary ]() {
          const QSemaphoreReleaser _( sem );
          const QString & dictionaryName = QString::fromUtf8( dictionary->getName().c_str() );
          qDebug() << "[FULLTEXT] checking fts for the dictionary:" << dictionaryName;
          emit sendNowIndexingName( dictionaryName );
          dictionary->makeFTSIndex( isCancelled );
        } );
        synchronizer.addFuture( f );
      }
    }
    qDebug() << "waiting for all the fts creation to finish.";
    synchronizer.waitForFinished();
    qDebug() << "finished/cancel all the fts creation";
    timerThread->quit();
    timerThread->wait();
  }
  catch ( std::exception & ex ) {
    qWarning( "Exception occurred while full-text search: %s", ex.what() );
  }
  emit sendNowIndexingName( QString() );
}

void Indexing::timeout()
{
  QString indexingDicts;
  for ( const auto & dictionary : dictionaries ) {
    if ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      break;
    }
    //Finished, clear the msg.
    if ( dictionary->haveFTSIndex() ) {
      continue;
    }
    auto newProgress = dictionary->getIndexingFtsProgress();
    if ( newProgress > 0 && newProgress < 100 ) {
      if ( !indexingDicts.isEmpty() ) {
        indexingDicts.append( "," );
      }
      indexingDicts.append(
        QString( "%1......%%2" ).arg( QString::fromStdString( dictionary->getName() ) ).arg( newProgress ) );
    }
  }

  if ( !indexingDicts.isEmpty() ) {
    emit sendNowIndexingName( indexingDicts );
  }
}

FtsIndexing::FtsIndexing( const std::vector< sptr< Dictionary::Class > > & dicts ):
  dictionaries( dicts ),
  started( false )
{
}

void FtsIndexing::doIndexing()
{
  if ( started ) {
    stopIndexing();
  }

  if ( !started ) {
    while ( Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      isCancelled.deref();
    }

    Indexing * idx = new Indexing( isCancelled, dictionaries, indexingExited );

    connect( idx, &Indexing::sendNowIndexingName, this, &FtsIndexing::setNowIndexedName );

    QThreadPool::globalInstance()->start( idx );

    started = true;
  }
}

void FtsIndexing::stopIndexing()
{
  if ( started ) {
    if ( !Utils::AtomicInt::loadAcquire( isCancelled ) ) {
      isCancelled.ref();
    }

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

void addSortedHeadwords( QList< FtsHeadword > & base_list, const QList< FtsHeadword > & add_list )
{
  QList< FtsHeadword > list;

  if ( add_list.isEmpty() ) {
    return;
  }

  if ( base_list.isEmpty() ) {
    base_list = add_list;
    return;
  }

  list.reserve( base_list.size() + add_list.size() );

  QList< FtsHeadword >::iterator base_it      = base_list.begin();
  QList< FtsHeadword >::const_iterator add_it = add_list.constBegin();

  while ( base_it != base_list.end() || add_it != add_list.end() ) {
    if ( base_it == base_list.end() ) {
      while ( add_it != add_list.end() ) {
        list.append( *add_it );
        ++add_it;
      }
      break;
    }

    if ( add_it == add_list.end() ) {
      while ( base_it != base_list.end() ) {
        list.append( *base_it );
        ++base_it;
      }
      break;
    }

    if ( *add_it < *base_it ) {
      list.append( *add_it );
      ++add_it;
    }
    else if ( *add_it == *base_it ) {
      base_it->dictIDs.append( add_it->dictIDs );
      for ( QStringList::const_iterator itr = add_it->foundHiliteRegExps.constBegin();
            itr != add_it->foundHiliteRegExps.constEnd();
            ++itr ) {
        if ( !base_it->foundHiliteRegExps.contains( *itr ) ) {
          base_it->foundHiliteRegExps.append( *itr );
        }
      }
      ++add_it;
    }
    else {
      list.append( *base_it );
      ++base_it;
    }
  }

  base_list.swap( list );
}

FullTextSearchDialog::FullTextSearchDialog( QWidget * parent,
                                            Config::Class & cfg_,
                                            const std::vector< sptr< Dictionary::Class > > & dictionaries_,
                                            const std::vector< Instances::Group > & groups_,
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

  if ( cfg.preferences.fts.dialogGeometry.size() > 0 ) {
    restoreGeometry( cfg.preferences.fts.dialogGeometry );
  }

  setNewIndexingName( ftsIdx.nowIndexingName() );

  connect( &ftsIdx, &FtsIndexing::newIndexingName, this, &FullTextSearchDialog::setNewIndexingName );
  connect( GlobalBroadcaster::instance(),
           &GlobalBroadcaster::indexingDictionary,
           this,
           &FullTextSearchDialog::setNewIndexingName );

  ui.searchMode->addItem( tr( "Default" ), WholeWords );
  ui.searchMode->addItem( tr( "Wildcards" ), Wildcards );

  ui.searchLine->setToolTip( tr( "Support xapian search syntax, such as AND OR +/- etc." ) );

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

  connect( &helpAction, &QAction::triggered, this, []() {
    Help::openHelpWebpage( Help::section::ui_fulltextserch );
  } );
  connect( ui.helpButton, &QAbstractButton::clicked, &helpAction, &QAction::trigger );

  addAction( &helpAction );

  ui.headwordsView->installEventFilter( this );

  delegate = new WordListItemDelegate( ui.headwordsView->itemDelegate() );
  if ( delegate ) {
    ui.headwordsView->setItemDelegate( delegate );
  }

  ui.searchLine->selectAll();
}

void FullTextSearchDialog::setSearchText( const QString & text )
{
  ui.searchLine->setText( text );
}

FullTextSearchDialog::~FullTextSearchDialog()
{
  if ( delegate ) {
    delegate->deleteLater();
  }
}

void FullTextSearchDialog::stopSearch()
{
  if ( !searchReqs.empty() ) {
    for ( std::list< sptr< Dictionary::DataRequest > >::iterator it = searchReqs.begin(); it != searchReqs.end();
          ++it ) {
      if ( !( *it )->isFinished() ) {
        ( *it )->cancel();
      }
    }

    while ( searchReqs.size() ) {
      QApplication::processEvents();
    }
  }
}

void FullTextSearchDialog::showDictNumbers()
{
  ui.totalDicts->setText( QString::number( activeDicts.size() ) );

  unsigned ready = 0, toIndex = 0;
  for ( unsigned x = 0; x < activeDicts.size(); x++ ) {
    if ( activeDicts.at( x )->haveFTSIndex() ) {
      ready++;
    }
    else {
      toIndex++;
    }
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
  ui.nowIndexingLabel->setText( tr( "Now indexing: " ) + ( name.isEmpty() ? tr( "None" ) : name ) );
  showDictNumbers();
}

void FullTextSearchDialog::accept()
{
  const int mode = ui.searchMode->itemData( ui.searchMode->currentIndex() ).toInt();

  model->clear();
  matchedCount = 0;
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

  if ( activeDicts.empty() ) {
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
  while ( searchReqs.size() ) {
    std::list< sptr< Dictionary::DataRequest > >::iterator it;
    for ( it = searchReqs.begin(); it != searchReqs.end(); ++it ) {
      if ( ( *it )->isFinished() ) {
        qDebug( "one finished." );

        QString errorString = ( *it )->getErrorString();

        if ( ( *it )->dataSize() >= 0 || errorString.size() ) {
          QList< FtsHeadword > * headwords;
          if ( (unsigned)( *it )->dataSize() >= sizeof( headwords ) ) {
            QList< FtsHeadword > hws;
            try {
              ( *it )->getDataSlice( 0, sizeof( headwords ), &headwords );
              hws.swap( *headwords );
              std::sort( hws.begin(), hws.end() );
              delete headwords;
              addSortedHeadwords( allHeadwords, hws );
            }
            catch ( std::exception & e ) {
              qWarning( "getDataSlice error: %s", e.what() );
            }
          }
        }
        break;
      }
    }
    if ( it != searchReqs.end() ) {
      searchReqs.erase( it );
      continue;
    }
    else {
      break;
    }
  }

  if ( !allHeadwords.isEmpty() ) {
    model->addResults( QModelIndex(), allHeadwords );
    if ( results.size() > matchedCount ) {
      ui.articlesFoundLabel->setText( tr( "Articles found: " ) + QString::number( results.size() ) );
    }
  }

  if ( searchReqs.empty() ) {
    ui.searchProgressBar->hide();
    ui.OKButton->setEnabled( true );
    QApplication::beep();
  }
}

void FullTextSearchDialog::matchCount( int _matchCount )
{
  matchedCount += _matchCount;
  ui.articlesFoundLabel->setText( tr( "Articles found: " ) + QString::number( matchedCount ) );
}

void FullTextSearchDialog::reject()
{
  if ( !searchReqs.empty() ) {
    stopSearch();
  }
  else {
    saveData();
    emit closeDialog();
  }
}

void FullTextSearchDialog::itemClicked( const QModelIndex & idx )
{
  if ( idx.isValid() && idx.row() < results.size() ) {
    QString headword = results[ idx.row() ].headword;
    QRegularExpression reg;
    auto searchText = ui.searchLine->text();
    searchText.replace( RX::Ftx::tokenBoundary, " " );

    if ( !searchText.isEmpty() ) {
      reg = QRegularExpression( searchText, QRegularExpression::CaseInsensitiveOption );
    }

    emit showTranslationFor( headword, results[ idx.row() ].dictIDs, reg, false );
  }
}

void FullTextSearchDialog::updateDictionaries()
{
  activeDicts.clear();

  // Find the given group

  const Instances::Group * activeGroup = 0;

  for ( unsigned x = 0; x < groups.size(); ++x ) {
    if ( groups[ x ].id == group ) {
      activeGroup = &groups[ x ];
      break;
    }
  }

  // If we've found a group, use its dictionaries; otherwise, use the global
  // heap.
  const std::vector< sptr< Dictionary::Class > > & groupDicts = activeGroup ? activeGroup->dictionaries : dictionaries;

  // Exclude muted dictionaries

  const Config::Group * grp = cfg.getGroup( group );
  const Config::MutedDictionaries * mutedDicts;

  if ( group == GroupId::AllGroupId ) {
    mutedDicts = &cfg.mutedDictionaries;
  }
  else {
    mutedDicts = grp ? &grp->mutedDictionaries : 0;
  }

  if ( mutedDicts && !mutedDicts->isEmpty() ) {
    activeDicts.reserve( groupDicts.size() );
    for ( unsigned x = 0; x < groupDicts.size(); ++x ) {
      if ( groupDicts[ x ]->canFTS() && !mutedDicts->contains( QString::fromStdString( groupDicts[ x ]->getId() ) ) ) {
        activeDicts.push_back( groupDicts[ x ] );
      }
    }
  }
  else {
    for ( unsigned x = 0; x < groupDicts.size(); ++x ) {
      if ( groupDicts[ x ]->canFTS() ) {
        activeDicts.push_back( groupDicts[ x ] );
      }
    }
  }

  showDictNumbers();
}

bool FullTextSearchDialog::eventFilter( QObject * obj, QEvent * ev )
{
  if ( obj == ui.headwordsView && ev->type() == QEvent::KeyPress ) {
    QKeyEvent * kev = static_cast< QKeyEvent * >( ev );
    if ( kev->key() == Qt::Key_Return || kev->key() == Qt::Key_Enter ) {
      itemClicked( ui.headwordsView->currentIndex() );
      return true;
    }
  }
  return QDialog::eventFilter( obj, ev );
}

/// HeadwordsListModel

int HeadwordsListModel::rowCount( const QModelIndex & ) const
{
  return headwords.size();
}

QVariant HeadwordsListModel::data( const QModelIndex & index, int role ) const
{
  if ( index.row() < 0 ) {
    return QVariant();
  }

  const FtsHeadword & head = headwords[ index.row() ];

  if ( head.headword.isEmpty() ) {
    return QVariant();
  }

  switch ( role ) {
    case Qt::ToolTipRole: {
      QString tt;
      for ( int x = 0; x < head.dictIDs.size(); x++ ) {
        if ( x != 0 ) {
          tt += "<br>";
        }

        int n = getDictIndex( head.dictIDs[ x ] );
        if ( n != -1 ) {
          tt += QString::fromUtf8( dictionaries[ n ]->getName().c_str() );
        }
      }
      return tt;
    }

    case Qt::DisplayRole:
      return head.headword;

    case Qt::EditRole:
      return head.headword;

    default:;
  }

  return QVariant();
}

void HeadwordsListModel::addResults( const QModelIndex & parent, const QList< FtsHeadword > & hws )
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

int HeadwordsListModel::getDictIndex( const QString & id ) const
{
  std::string dictID( id.toUtf8().data() );
  for ( unsigned x = 0; x < dictionaries.size(); x++ ) {
    if ( dictionaries[ x ]->getId().compare( dictID ) == 0 ) {
      return x;
    }
  }
  return -1;
}


bool FtsHeadword::operator<( const FtsHeadword & other ) const
{
  QString first  = Utils::trimQuotes( headword );
  QString second = Utils::trimQuotes( other.headword );

  int result = first.localeAwareCompare( second );
  if ( result ) {
    return result < 0;
  }

  // Headwords without quotes are equal

  if ( first.size() != headword.size() || second.size() != other.headword.size() ) {
    return headword.localeAwareCompare( other.headword ) < 0;
  }

  return false;
}

} // namespace FTS
