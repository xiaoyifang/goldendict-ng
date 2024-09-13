#include <QStyleOptionViewItem>

#include "delegate.hh"

WordListItemDelegate::WordListItemDelegate( QAbstractItemDelegate * delegate ):
  QStyledItemDelegate()
{
  mainDelegate = static_cast< QStyledItemDelegate * >( delegate );
}

void WordListItemDelegate::paint( QPainter * painter,
                                  const QStyleOptionViewItem & option,
                                  const QModelIndex & index ) const
{
  QStyleOptionViewItem opt = option;
  initStyleOption( &opt, index );
  if ( opt.text.isRightToLeft() ) {
    opt.direction = Qt::RightToLeft;
  }
  else {
    opt.direction = Qt::LeftToRight;
  }
  mainDelegate->paint( painter, opt, index );
}
