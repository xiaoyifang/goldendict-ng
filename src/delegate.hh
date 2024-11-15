#pragma once

#include <QAbstractItemDelegate>
#include <QStyledItemDelegate>

class WordListItemDelegate: public QStyledItemDelegate
{
public:
  explicit WordListItemDelegate( QAbstractItemDelegate * delegate );
  virtual void paint( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;

private:
  QStyledItemDelegate * mainDelegate;
};
