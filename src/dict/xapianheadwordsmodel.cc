/* This file is (c) 2024 xiaoyifang
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "xapianheadwordsmodel.hh"
#include "xapianidx.hh"
#include <QDebug>

namespace XapianIndexing {

constexpr quint32 PageSize = 200;

XapianHeadwordListModel::XapianHeadwordListModel( QObject * parent )
  : QAbstractListModel( parent )
{
}

void XapianHeadwordListModel::setDict( sptr< Dictionary::Class > dict )
{
  beginResetModel();
  m_dict      = dict;
  m_filter    = QString();
  m_headwords.clear();
  m_totalCount = 0;
  endResetModel();

  if ( m_dict ) {
    fetchMore( QModelIndex() );
  }
}

int XapianHeadwordListModel::rowCount( const QModelIndex & parent ) const
{
  return parent.isValid() ? 0 : m_headwords.size();
}

QVariant XapianHeadwordListModel::data( const QModelIndex & index, int role ) const
{
  if ( !index.isValid() || index.row() >= m_headwords.size() ) {
    return QVariant();
  }

  if ( role == Qt::DisplayRole ) {
    return m_headwords.at( index.row() );
  }

  return QVariant();
}

void XapianHeadwordListModel::setFilter( const QString & filter )
{
  beginResetModel();
  m_filter = filter;
  m_headwords.clear();
  m_totalCount = 0;
  endResetModel();

  fetchMore( QModelIndex() );
}

bool XapianHeadwordListModel::canFetchMore( const QModelIndex & parent ) const
{
  if ( parent.isValid() )
    return false;
  return m_headwords.size() < m_totalCount;
}

void XapianHeadwordListModel::fetchMore( const QModelIndex & parent )
{
  if ( parent.isValid() || !m_dict )
    return;

  quint32 currentCount = m_headwords.size();
  std::map< QString, uint32_t > newWords;
  if ( m_filter.isEmpty() ) {
    newWords = getIndexedWordsByOffset( m_dict->getId(), currentCount, PageSize, m_totalCount );
  }
  else {
    newWords = searchIndexedWordsByOffset( m_dict->getId(), m_filter.toStdString(), currentCount, PageSize, m_totalCount );
  }

  if ( !newWords.empty() ) {
    beginInsertRows( QModelIndex(), currentCount, currentCount + newWords.size() - 1 );
    for ( const auto & [ word, _ ] : newWords ) {
      m_headwords.append( word );
    }
    endInsertRows();
  }
}

} // namespace XapianIndexing
