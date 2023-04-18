/* This file is (c) 2013 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */


#include "wordlist.hh"

WordList::WordList( QWidget * parent ) : QListWidget( parent )
, listItemDelegate( itemDelegate() )
{
  wordFinder = 0;
  translateLine = 0;
  setItemDelegate( &listItemDelegate );
}

void WordList::attachFinder( WordFinder * finder )
{
  // qDebug() << "Attaching the word finder..." << finder;

  if ( wordFinder == finder )
    return;

  if ( wordFinder )
  {
    disconnect( wordFinder, &WordFinder::updated, this, &WordList::prefixMatchUpdated );
    disconnect( wordFinder, &WordFinder::finished, this, &WordList::prefixMatchFinished );
  }

  wordFinder = finder;

  connect( wordFinder, &WordFinder::updated, this, &WordList::prefixMatchUpdated );
  connect( wordFinder, &WordFinder::finished, this, &WordList::prefixMatchFinished );
}

void WordList::prefixMatchUpdated()
{
  updateMatchResults( false );
}

void WordList::prefixMatchFinished()
{
  updateMatchResults( true );
}

void WordList::updateMatchResults( bool finished )
{
  WordFinder::SearchResults const & results = wordFinder->getResults();

  setUpdatesEnabled( false );
  //clear all existed items
  clear();

  for( unsigned x = 0; x < results.size(); ++x )
  {
    QListWidgetItem * i = new QListWidgetItem( results[ x ].first, this );
    i->setToolTip( results[ x ].first );

    if( results[ x ].second )
    {
      QFont f = i->font();
      f.setItalic( true );
      i->setFont( f );
    }

    i->setTextAlignment( Qt::AlignLeft );
  }

  if ( count() )
  {
    scrollToItem( item( 0 ), QAbstractItemView::PositionAtTop );
    setCurrentItem( 0, QItemSelectionModel::Clear );
  }

  setUpdatesEnabled( true );

  if ( finished )
  {
    unsetCursor();

    refreshTranslateLine();

    if ( !wordFinder->getErrorString().isEmpty() )
      emit statusBarMessage(tr("WARNING: %1").arg(wordFinder->getErrorString()),
                            20000, QPixmap(":/icons/error.svg"));
  }

  if( !results.empty() && results.front().first.isRightToLeft() )
    setLayoutDirection( Qt::RightToLeft );
  else
    setLayoutDirection( Qt::LeftToRight );

  emit contentChanged();
}

void WordList::refreshTranslateLine()
{
  if ( !translateLine )
    return;

  // Visually mark the input line to mark if there's no results
  bool setMark = wordFinder->getResults().empty() && !wordFinder->wasSearchUncertain();

  if ( translateLine->property( "noResults" ).toBool() != setMark )
  {
    translateLine->setProperty( "noResults", setMark );

    Utils::Widget::setNoResultColor( translateLine, setMark );
  }

}
