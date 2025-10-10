/* This file is (c) 2024 xiaoyifang
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "dict/dictionary.hh"
#include <QAbstractListModel>
#include <QList>
#include <QString>

namespace XapianIndexing {

class XapianHeadwordListModel : public QAbstractListModel
{
  Q_OBJECT

public:
  explicit XapianHeadwordListModel( QObject * parent = nullptr );

  void setDict( sptr< Dictionary::Class > dict );

  int rowCount( const QModelIndex & parent = QModelIndex() ) const override;
  QVariant data( const QModelIndex & index, int role = Qt::DisplayRole ) const override;

  void setFilter( const QString & filter );

protected:
  bool canFetchMore( const QModelIndex & parent ) const override;
  void fetchMore( const QModelIndex & parent ) override;

private:
  sptr< Dictionary::Class > m_dict;
  QString m_filter;
  QList< QString > m_headwords;
  quint32 m_totalCount = 0;
};

} // namespace XapianIndexing