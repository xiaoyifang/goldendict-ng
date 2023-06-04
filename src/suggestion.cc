/* This file is (c) 2013 Tvangeste <i.4m.l33t@yandex.ru>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */


#include "suggestion.hh"

Suggestion::Suggestion( QObject * parent ) : QStringListModel( parent )
{
  wordFinder = 0;
  translateLine = 0;
  completer     = new QCompleter( this, this );
  completer->setCaseSensitivity( Qt::CaseInsensitive );
  completer->setCompletionMode( QCompleter::InlineCompletion );
}

QWidget * Suggestion::completerWidget()
{
  return completer->widget();
}


void Suggestion::attachFinder( WordFinder * finder )
{
  qDebug() << "Attaching the word finder..." << finder;

  if ( wordFinder == finder )
    return;

  if ( wordFinder )
  {
    disconnect( wordFinder, &WordFinder::updated, this, &Suggestion::prefixMatchUpdated );
    disconnect( wordFinder, &WordFinder::finished, this, &Suggestion::prefixMatchFinished );
  }

  wordFinder = finder;

  connect( wordFinder, &WordFinder::updated, this, &Suggestion::prefixMatchUpdated );
  connect( wordFinder, &WordFinder::finished, this, &Suggestion::prefixMatchFinished );
}

void Suggestion::prefixMatchUpdated()
{
  updateMatchResults( false );
}

void Suggestion::prefixMatchFinished()
{
  updateMatchResults( true );
}

void Suggestion::updateMatchResults( bool finished )
{
  WordFinder::SearchResults const & results = wordFinder->getResults();

  QStringList _results;

  for( unsigned x = 0; x < results.size(); ++x )
  {
    _results << results[x].first;

  }

  setStringList(_results);


  if ( finished )
  {

    refreshTranslateLine();

    if ( !wordFinder->getErrorString().isEmpty() )
      emit statusBarMessage(tr("WARNING: %1").arg(wordFinder->getErrorString()),
                            20000, QPixmap(":/icons/error.svg"));
  }



  emit contentChanged();
}

void Suggestion::refreshTranslateLine()
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
