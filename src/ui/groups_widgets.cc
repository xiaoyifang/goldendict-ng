/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "groups_widgets.hh"

#include "instances.hh"
#include "config.hh"
#include "langcoder.hh"
#include "language.hh"
#include "metadata.hh"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QVector>

using std::vector;

/// DictGroupWidget

DictGroupWidget::DictGroupWidget( QWidget * parent,
                                  vector< sptr< Dictionary::Class > > const & dicts,
                                  Config::Group const & group ):
  QWidget( parent ),
  groupId( group.id )
{
  ui.setupUi( this );
  ui.dictionaries->populate( Instances::Group( group, dicts, Config::Group() ).dictionaries, dicts );

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  ui.shortcut->setClearButtonEnabled(true);
  ui.clearShortCut->hide();
#endif

  // Populate icons' list

  QStringList icons = QDir( ":/flags/" ).entryList( QDir::Files, QDir::NoSort );

  ui.groupIcon->addItem( tr( "None" ), "" );

  const bool usesIconData = !group.iconData.isEmpty();

  if ( !usesIconData )
    ui.groupIcon->addItem( tr( "From file..." ), "" );
  else
    ui.groupIcon->addItem( Instances::iconFromData( group.iconData ), group.icon, group.icon );

  for( int x = 0; x < icons.size(); ++x )
  {
    QString n( icons[ x ] );
    n.chop( 4 );
    n[ 0 ] = n[ 0 ].toUpper();

    ui.groupIcon->addItem( QIcon( ":/flags/" + icons[ x ] ), n, icons[ x ] );

    if ( !usesIconData && icons[ x ] == group.icon )
      ui.groupIcon->setCurrentIndex( x + 2 );
  }

  if ( usesIconData )
    ui.groupIcon->setCurrentIndex( 1 );

  ui.shortcut->setKeySequence( group.shortcut );

  ui.favoritesFolder->setText( group.favoritesFolder );

  connect( ui.groupIcon, &QComboBox::activated, this, &DictGroupWidget::groupIconActivated, Qt::QueuedConnection );

  ui.dictionaries->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( ui.dictionaries, &QWidget::customContextMenuRequested, this, &DictGroupWidget::showDictInfo );

  connect( ui.dictionaries, &QAbstractItemView::doubleClicked, this, &DictGroupWidget::removeCurrentItem );
}

void DictGroupWidget::groupIconActivated( int index )
{
  if ( index == 1 ) {
    QList< QByteArray > supImageFormats = QImageReader::supportedImageFormats();

    QString formatList( " (" );

    for ( const auto & supImageFormat : supImageFormats )
      formatList += "*." + QString::fromLatin1( supImageFormat ) + " ";

    formatList.chop( 1 );
    formatList.append( ")" );

    const QString chosenFile =
      QFileDialog::getOpenFileName( this,
                                    tr( "Choose a file to use as group icon" ),
                                    QString(),
                                    tr( "Images" ) + formatList + ";;" + tr( "All files" ) + " (*.*)" );

    if ( !chosenFile.isEmpty() ) {
      const QIcon icon( chosenFile );

      if ( icon.isNull() )
        QMessageBox::critical( this, tr( "Error" ), tr( "Can't read the specified image file." ) );
      else {
        ui.groupIcon->setItemIcon( 1, icon );

        const QString baseName = QFileInfo( chosenFile ).completeBaseName();
        ui.groupIcon->setItemText( 1, baseName );
        ui.groupIcon->setItemData( 1, baseName );
      }
    }
  }
}

Config::Group DictGroupWidget::makeGroup() const
{
  Instances::Group g( "" );

  g.id = groupId;

  g.dictionaries = ui.dictionaries->getCurrentDictionaries();

  const int currentIndex = ui.groupIcon->currentIndex();

  if ( currentIndex == 1 ) // File
    g.iconData = ui.groupIcon->itemIcon( currentIndex );

  g.icon = ui.groupIcon->itemData( currentIndex ).toString();

  g.shortcut = ui.shortcut->keySequence();

  g.favoritesFolder = ui.favoritesFolder->text().replace( '\\', '/' );

  return g.makeConfigGroup();
}

void DictGroupWidget::showDictInfo( QPoint const & pos )
{
  const QVariant data = ui.dictionaries->getModel()->data( ui.dictionaries->indexAt( pos ), Qt::EditRole );
  QString id;
  if( data.canConvert< QString >() )
    id = data.toString();

  if( !id.isEmpty() )
  {
    vector< sptr< Dictionary::Class > > const & dicts = ui.dictionaries->getCurrentDictionaries();
    unsigned n;
    for( n = 0; n < dicts.size(); n++ )
      if( id.compare( QString::fromUtf8( dicts.at( n )->getId().c_str() ) ) == 0 )
        break;
    if( n < dicts.size() )
      emit showDictionaryInfo( id );
  }
}

void DictGroupWidget::removeCurrentItem( QModelIndex const & index )
{
  (void)index;
  ui.dictionaries->getModel()->removeSelectedRows( ui.dictionaries->selectionModel() );
}

/// DictListModel

void DictListModel::populate(
  std::vector< sptr< Dictionary::Class > > const & active,
  std::vector< sptr< Dictionary::Class > > const & available )
{
  dictionaries = active;
  allDicts = &available;

  beginResetModel();
  endResetModel();
}

void DictListModel::populate(
  std::vector< sptr< Dictionary::Class > > const & active )
{
  dictionaries = active;
  beginResetModel();
  endResetModel();
}

void DictListModel::setAsSource()
{
  isSource = true;
}

std::vector< sptr< Dictionary::Class > > const &
  DictListModel::getCurrentDictionaries() const
{
  return dictionaries;
}

Qt::ItemFlags DictListModel::flags( QModelIndex const & index ) const
{
  const Qt::ItemFlags defaultFlags = QAbstractListModel::flags( index );

  if (index.isValid())
     return Qt::ItemIsDragEnabled | defaultFlags;
  else
     return Qt::ItemIsDropEnabled | defaultFlags;
}

int DictListModel::rowCount( QModelIndex const & ) const
{
  return dictionaries.size();
}

QVariant DictListModel::data( QModelIndex const & index, int role ) const
{
  if( index.row() < 0 )
    return QVariant();
  
  sptr< Dictionary::Class > const & item = dictionaries[ index.row() ];

  if ( !item )
    return QVariant();

  switch ( role )
  {
    case Qt::ToolTipRole: {
      QString tt = "<b>" + QString::fromUtf8( item->getName().c_str() ) + "</b>";

      const QString lfrom( Language::localizedNameForId( item->getLangFrom() ) );
      const QString lto( Language::localizedNameForId( item->getLangTo() ) );
      if ( !lfrom.isEmpty() ) {
        if ( lfrom == lto )
          tt += "<br>" + lfrom;
        else
          tt += "<br>" + lfrom + " - " + lto;
      }

      int entries = item->getArticleCount();
      if ( !entries )
        entries = item->getWordCount();
      if ( entries )
        tt += "<br>" + tr( "%1 entries" ).arg( entries );

      const std::vector< std::string > & dirs = item->getDictionaryFilenames();

      if ( dirs.size() )
      {
        tt += "<hr>";
        tt += dirs.at( 0 ).c_str();
      }

      tt.replace( " ", "&nbsp;" );
      return tt;
    }

    case Qt::DisplayRole :
      return QString::fromUtf8( item->getName().c_str() );

    case Qt::EditRole :
      return QString::fromUtf8( item->getId().c_str() );

    case Qt::DecorationRole:
      // make all icons of the same size to avoid visual size/alignment problems
      return item->getIcon();

    default:;
  }

  return QVariant();
}

bool DictListModel::insertRows( int row, int count, const QModelIndex & parent )
{
  if ( isSource )
    return false;

  beginInsertRows( parent, row, row + count - 1 );
  dictionaries.insert( dictionaries.begin() + row, count,
                       sptr< Dictionary::Class >() );
  endInsertRows();
  emit contentChanged();
  return true;
}

void DictListModel::addRow(const QModelIndex & parent, sptr< Dictionary::Class > dict)
{
  for ( const auto & dictionary : dictionaries ) {
    if ( dictionary->getId() == dict->getId() )
      return;
  }

  beginInsertRows( parent, dictionaries.size(), dictionaries.size() + 1 );
  dictionaries.push_back( dict );
  endInsertRows();
  emit contentChanged();
}

bool DictListModel::removeRows( int row, int count,
                                const QModelIndex & parent )
{
  if ( isSource )
    return false;

  beginRemoveRows( parent, row, row + count - 1 );
  dictionaries.erase( dictionaries.begin() + row,
                      dictionaries.begin() + row + count );
  endRemoveRows();
  emit contentChanged();
  return true;
}

bool DictListModel::setData( QModelIndex const & index, const QVariant & value,
                             int role )
{
  if ( isSource || !allDicts || !index.isValid() ||
       index.row() >= (int)dictionaries.size() )
    return false;

  if ( ( role == Qt::DisplayRole ) || ( role ==  Qt::DecorationRole ) )
  {
    // Allow changing that, but do nothing
    return true;
  }

  if ( role == Qt::EditRole )
  {
    Config::Group g;

    g.dictionaries.push_back( Config::DictionaryRef( value.toString(), QString() ) );

    const Instances::Group i( g, *allDicts, Config::Group() );

    if ( i.dictionaries.size() == 1 )
    {
      // Found that dictionary
      dictionaries[ index.row() ] = i.dictionaries.front();

      emit dataChanged( index, index );

      return true;
    }
  }

  return false;
}

Qt::DropActions DictListModel::supportedDropActions() const
{
  return Qt::MoveAction;
}

void DictListModel::removeSelectedRows( QItemSelectionModel * source )
{
  if ( !source )
    return;

  const QModelIndexList rows = source->selectedRows();

  if ( !rows.count() )
    return;

  for ( int i = rows.count()-1; i >= 0; --i )
  {
    dictionaries.erase( dictionaries.begin() + rows.at( i ).row() );
  }

  beginResetModel();
  endResetModel();
  emit contentChanged();
}

void DictListModel::addSelectedUniqueFromModel( QItemSelectionModel * source )
{
  if ( !source )
    return;

  const QModelIndexList rows = source->selectedRows();

  if ( !rows.count() )
    return;

  const QSortFilterProxyModel * proxyModel = dynamic_cast< const QSortFilterProxyModel * > ( source->model() );

  const DictListModel * baseModel;

  if ( proxyModel )
  {
    baseModel = dynamic_cast< const DictListModel * > ( proxyModel->sourceModel() );
  }
  else {
    baseModel = dynamic_cast< const DictListModel * >( source->model() );
  }

  if ( !baseModel )
    return;

  QVector< std::string > list;
  QVector< std::string > dicts;
  for ( const auto & dictionarie : dictionaries )
    dicts.append( dictionarie->getId() );

  for ( int i = 0; i < rows.count(); i++ ) {
    QModelIndex idx = proxyModel ? proxyModel->mapToSource( rows.at( i ) ) : rows.at( i );
    std::string id  = baseModel->dictionaries.at( idx.row() )->getId();

    if ( !dicts.contains( id ) )
      list.append( id );
  }

  if ( list.empty() )
    return;

  for ( const auto & j : list ) {
    for ( const auto & allDict : *allDicts ) {
      if ( allDict->getId() == j ) {
        dictionaries.push_back( allDict );
        break;
      }
    }
  }

  beginResetModel();
  endResetModel();
  emit contentChanged();
}

void DictListModel::filterDuplicates()
{
  QSet< QString > ids;
  bool doReset = false;

  for ( unsigned i = 0; i < dictionaries.size(); i++ )
  {
    QString id = QString::fromStdString( dictionaries.at( i )->getId() );

    if ( ids.contains( id ) )
    {
      dictionaries.erase( dictionaries.begin() + i-- );
      doReset = true;
      continue;
    }

    ids.insert( id );
  }

  if ( doReset )
  {
    beginResetModel();
    endResetModel();
    emit contentChanged();
  }
}

/// DictListWidget

DictListWidget::DictListWidget( QWidget * parent ): QListView( parent ),
  model( this )
{
  setModel( &model );

  setSelectionMode( ExtendedSelection );

  setDragEnabled( true );
  setAcceptDrops( true );
  setDropIndicatorShown( true );
}

void DictListWidget::populate(
  std::vector< sptr< Dictionary::Class > > const & active,
  std::vector< sptr< Dictionary::Class > > const & available )
{
  model.populate( active, available );
}

void DictListWidget::populate(
  std::vector< sptr< Dictionary::Class > > const & active )
{
  model.populate( active );
}

void DictListWidget::setAsSource()
{
  setDropIndicatorShown( false );
  model.setAsSource();
}

std::vector< sptr< Dictionary::Class > > const &
  DictListWidget::getCurrentDictionaries() const
{
  return model.getCurrentDictionaries();
}

void DictListWidget::dropEvent( QDropEvent * event )
{
  const auto sourceList = dynamic_cast< DictListWidget * >( event->source() );

  QListView::dropEvent( event );

  if ( sourceList != this )
  {
    model.filterDuplicates();
  }
}

void DictListWidget::focusInEvent( QFocusEvent * )
{
  emit gotFocus();
}

void DictListWidget::rowsInserted( QModelIndex const & parent, int start, int end )
{
  QListView::rowsInserted( parent, start, end );

  // When inserting new rows, make the first of them current
  selectionModel()->setCurrentIndex( model.index( start, 0, parent ),
                                     QItemSelectionModel::NoUpdate );
}

void DictListWidget::rowsAboutToBeRemoved( QModelIndex const & parent, int start, int end )
{
  // When removing rows, if the current row is among the removed ones, select
  // an item just before the first row to be removed, if there's one.

  if ( const QModelIndex current = currentIndex();
       current.isValid() && current.row() && current.row() >= start && current.row() <= end )
    selectionModel()->setCurrentIndex( model.index( current.row() - 1, 0, parent ), QItemSelectionModel::NoUpdate );

  QListView::rowsAboutToBeRemoved( parent, start, end );
}


// DictGroupsWidget

DictGroupsWidget::DictGroupsWidget( QWidget * parent ):
  QTabWidget( parent ),
  nextId( 1 ),
  allDicts( nullptr ),
  activeDicts( nullptr )
{
  setMovable( true );
  setContextMenuPolicy( Qt::CustomContextMenu );
  connect( this, &QWidget::customContextMenuRequested, this, &DictGroupsWidget::contextMenu );

  setElideMode( Qt::ElideNone );
  setUsesScrollButtons( true );
}

namespace {

QString escapeAmps( QString const & str )
{
  QString result( str );
  result.replace( "&", "&&" );
  return result;
}

QString unescapeAmps( QString const & str )
{
  QString result( str );
  result.replace( "&&", "&" );
  return result;
}

}

void DictGroupsWidget::populate( Config::Groups const & groups,
                                 vector< sptr< Dictionary::Class > > const & allDicts_,
                                 vector< sptr< Dictionary::Class > > const & activeDicts_ )
{
  removeAllGroups();

  allDicts = &allDicts_;
  activeDicts = &activeDicts_;

  for( int x = 0; x < groups.size(); ++x )
  {
    const auto gr = new DictGroupWidget( this, *allDicts, groups[ x ] );
    addTab( gr, escapeAmps( groups[ x ].name ) );
    connect( gr, &DictGroupWidget::showDictionaryInfo,this, &DictGroupsWidget::showDictionaryInfo );
    connect( gr->getModel(), &DictListModel::contentChanged, this, &DictGroupsWidget::tabDataChanged );

    QString toolTipStr = "\"" + tabText( x ) + "\"\n" + tr( "Dictionaries: " )
      + QString::number( getModelAt( x )->getCurrentDictionaries().size() );
    setTabToolTip( x, toolTipStr );
  }

  nextId = groups.nextId;

  setCurrentIndex( 0 );
}

/// Creates groups from what is currently set up
Config::Groups DictGroupsWidget::makeGroups() const
{
  Config::Groups result;

  result.nextId = nextId;

  for( int x = 0; x < count(); ++x )
  {
    result.push_back( dynamic_cast< DictGroupWidget & >( *widget( x ) ).makeGroup() );
    result.back().name = unescapeAmps( tabText( x ) );
  }

  return result;
}

DictListModel * DictGroupsWidget::getCurrentModel() const
{
  const int current = currentIndex();

  if ( current >= 0 )
  {
    const auto w = (DictGroupWidget *)widget( current );
    return w->getModel();
  }

  return nullptr;
}

DictListModel * DictGroupsWidget::getModelAt( int current ) const
{
  if ( current >= 0 && current < count() ) {
    const auto w = (DictGroupWidget *)widget( current );
    return w->getModel();
  }

  return nullptr;
}

QItemSelectionModel * DictGroupsWidget::getCurrentSelectionModel() const
{
  const int current = currentIndex();

  if ( current >= 0 )
  {
    const auto w = (DictGroupWidget *)widget( current );
    return w->getSelectionModel();
  }

  return nullptr;
}


int DictGroupsWidget::addNewGroup( QString const & name )
{
  if ( !allDicts )
    return 0;

  Config::Group newGroup;

  newGroup.id = nextId++;

  const auto gr = new DictGroupWidget( this, *allDicts, newGroup );
  const int idx = insertTab( currentIndex() + 1, gr, escapeAmps( name ) );
  connect( gr, &DictGroupWidget::showDictionaryInfo, this, &DictGroupsWidget::showDictionaryInfo );

  connect( gr->getModel(), &DictListModel::contentChanged, this, &DictGroupsWidget::tabDataChanged );

  const QString toolTipStr = "\"" + tabText( idx ) + "\"\n" + tr( "Dictionaries: " )
    + QString::number( getModelAt( idx )->getCurrentDictionaries().size() );
  setTabToolTip( idx, toolTipStr );
  return idx;
}

int DictGroupsWidget::addUniqueGroup( const QString & name )
{
  for( int n = 0; n < count(); n++ )
    if( tabText( n ) == name )
    {
      //      setCurrentIndex( n );
      return n;
    }

  return addNewGroup( name );
}

void DictGroupsWidget::addAutoGroups()
{
  if( !activeDicts )
    return;

  if ( QMessageBox::information( this, tr( "Confirmation" ),
         tr( "Are you sure you want to generate a set of groups "
             "based on language pairs?" ), QMessageBox::Yes,
             QMessageBox::Cancel ) != QMessageBox::Yes )
    return;

  QMap< QString, QVector< sptr<Dictionary::Class> > > dictMap;
  QMap< QString, QVector< sptr<Dictionary::Class> > > morphoMap;

  // Put active dictionaries into lists

  for ( const auto & dict : *activeDicts ) {
    int idFrom = dict->getLangFrom();
    int idTo = dict->getLangTo();
    if( idFrom == 0)
    {
      // Attempt to find language pair in dictionary name

      const QPair< quint32, quint32 > ids = LangCoder::findIdsForName( QString::fromUtf8( dict->getName().c_str() ) );
      idFrom = ids.first;
      idTo = ids.second;
    }

    QString name( tr( "Unassigned" ) );
    if ( idFrom != 0 && idTo != 0 )
    {
      QString lfrom = LangCoder::intToCode2( idFrom );
      QString lto = LangCoder::intToCode2( idTo );
      lfrom[ 0 ] = lfrom[ 0 ].toTitleCase();
      lto[ 0 ] = lto[ 0 ].toTitleCase();
      name = lfrom + " - " + lto;
    }
    else if( !dict->getDictionaryFilenames().empty() )
    {
      // Handle special case - morphology dictionaries

      QString fileName = QFileInfo( dict->getDictionaryFilenames()[ 0 ].c_str() ).fileName();
      if( fileName.endsWith( ".aff", Qt::CaseInsensitive ) )
      {
        QString code = fileName.left( 2 ).toLower();
        morphoMap[ code ].push_back( dict );
        continue;
      }
    }

    dictMap[ name ].push_back( dict );
  }

  QStringList groupList  = dictMap.keys();
  QStringList morphoList = morphoMap.keys();

  // Insert morphology dictionaries into corresponding lists

  for ( const auto & ln : morphoList ) {
    for ( const auto & gr : groupList )
      if ( ln.compare( gr.left( 2 ), Qt::CaseInsensitive ) == 0 ) {
        QVector< sptr< Dictionary::Class > > vdg = dictMap[ gr ];
        vdg += morphoMap[ ln ];
        dictMap[ gr ] = vdg;
      }
  }

  // Make groups

  for ( const auto & gr : groupList ) {
    const auto idx = addUniqueGroup( gr );

    // add dictionaries into the current group
    QVector< sptr< Dictionary::Class > > vd = dictMap[ gr ];
    DictListModel * model                 = getModelAt( idx );
    for( int i = 0; i < vd.count(); i++ )
      model->addRow(QModelIndex(), vd.at( i ) );
  }
}


void DictGroupsWidget::addAutoGroupsByFolders()
{
  if ( activeDicts->empty() ) {
    return;
  }

  auto cdUpWentWrong = [ this ]( const QString & path ) {
    QMessageBox::warning( this,
                          tr( "Auto group by folder failed." ),
                          tr( "The parent directory of %1 can not be reached." ).arg( path ) );
  };

  if ( QMessageBox::information( this,
                                 tr( "Confirmation" ),
                                 tr( "Are you sure you want to generate a set of groups "
                                     "based on containing folders?" ),
                                 QMessageBox::Yes,
                                 QMessageBox::Cancel )
       != QMessageBox::Yes ) {
    return;
  }

  // Map from dict to ContainerFolder's parent
  QMap< sptr< Dictionary::Class >, QString > dictToContainerFolder;

  for ( const auto & dict : *activeDicts ) {
    QDir c = dict->getContainingFolder();

    if ( !c.cdUp() ) {
      cdUpWentWrong( c.absolutePath() );
      return;
    }

    dictToContainerFolder.insert( dict, c.absolutePath() );
  }

  /*
  Check for duplicated ContainerFolder's parent folder names, and prepend the upper parent's name.

  .
  ├─epistularum
  │   └─Japanese   <- Group
  │       └─DictA  <- Dict Files's container folder
  |          └─ DictA Files
  ├─Mastameta
  │   └─Japanese   <- Group
  |       └─DictB  <- Dict Files's container folder
  |          └─ DictB Files

  will be grouped into epistularum/Japanese and Mastameta/Japanese
  */

  // set a dirname to true if there are duplications like `epistularum/Japanese` and `Mastameta/Japanese`
  QHash< QString, bool > dirNeedPrepend{};

  for ( const auto & path : dictToContainerFolder.values() ) {
    auto dir = QDir( path );
    if ( dirNeedPrepend.contains( dir.dirName() ) ) {
      dirNeedPrepend[ dir.dirName() ] = true;
    }
    else {
      dirNeedPrepend.insert( dir.dirName(), false );
    }
  }

  // map from GroupName to dicts
  QMultiMap< QString, sptr< Dictionary::Class > > groupToDicts;

  for ( const auto & dict : dictToContainerFolder.keys() ) {
    QDir path = dictToContainerFolder[ dict ];
    QString groupName;
    if ( !dirNeedPrepend[ path.dirName() ] ) {
      groupName = path.dirName();
    }
    else {
      QString directFolder = path.dirName();
      if ( !path.cdUp() ) {
        cdUpWentWrong( path.absolutePath() );
        return;
      }
      QString upperFolder = path.dirName();
      groupName           = upperFolder + "/" + directFolder;
    }

    groupToDicts.insert( groupName, dict );
  }

  // create and insert groups
  // modifying user's groups begins here
  addGroupBasedOnMap( groupToDicts );
}

void DictGroupsWidget::addGroupBasedOnMap( const QMultiMap<QString, sptr<Dictionary::Class>> & groupToDicts )
{
  for ( const auto & group : groupToDicts.uniqueKeys() ) {
    const auto idx        = addUniqueGroup( group );
    DictListModel * model = getModelAt( idx );

    for ( const auto & dict : groupToDicts.values( group ) ) {
      model->addRow( QModelIndex(), dict );
    }
  }
}

void DictGroupsWidget::groupsByMetadata()
{
  if ( activeDicts->empty() ) {
    return;
  }
  if ( QMessageBox::information( this,
                                 tr( "Confirmation" ),
                                 tr( "Are you sure you want to generate a set of groups based on metadata.toml?" ),
                                 QMessageBox::Yes,
                                 QMessageBox::Cancel )
       != QMessageBox::Yes ) {
    return;
  }
  // map from GroupName to dicts
  QMultiMap< QString, sptr< Dictionary::Class > > groupToDicts;

  for ( const auto & dict : *activeDicts ) {
    auto baseDir = dict->getContainingFolder();
    if ( baseDir.isEmpty() )
      continue;

    auto filePath = Utils::Path::combine( baseDir, "metadata.toml" );

    const auto dictMetaData = Metadata::load( filePath.toStdString() );
    if ( dictMetaData && dictMetaData->categories ) {
      for ( const auto & category : dictMetaData->categories.value() ) {
        auto group = QString::fromStdString( category ).trimmed();
        if ( group.isEmpty() ) {
          continue;
        }
        groupToDicts.insert( group, dict );
      }
    }
  }

  // create and insert groups
  // modifying user's groups begins here
  addGroupBasedOnMap( groupToDicts );
}


QString DictGroupsWidget::getCurrentGroupName() const
{
  const int current = currentIndex();

  if ( current >= 0 )
    return unescapeAmps( tabText( current ) );

  return QString();
}

void DictGroupsWidget::renameCurrentGroup( QString const & name )
{
  const int current = currentIndex();

  if ( current >= 0 )
    setTabText( current, escapeAmps( name ) );
}

void DictGroupsWidget::removeCurrentGroup()
{
  const int current = currentIndex();

  if ( current >= 0 ) {
    removeTab( current );
  }
}

void DictGroupsWidget::removeAllGroups()
{
  while ( count() )
  {
    const QWidget * w = widget( 0 );
    removeTab( 0 );
    delete w;
  }
}

void DictGroupsWidget::combineGroups( int source, int target )
{
  if( source < 0 || source >= count() || target < 0 || target >= count() )
    return;

  vector< sptr< Dictionary::Class > > const & dicts = getModelAt( source )->getCurrentDictionaries();

  const auto model = getModelAt( target );

  disconnect( model, &DictListModel::contentChanged, this, &DictGroupsWidget::tabDataChanged );

  for ( const auto & dict : dicts ) {
    model->addRow( QModelIndex(), dict );
  }

  connect( model, &DictListModel::contentChanged, this, &DictGroupsWidget::tabDataChanged );

  const QString toolTipStr = "\"" + tabText( target ) + "\"\n" + tr( "Dictionaries: " )
    + QString::number( model->getCurrentDictionaries().size() );
  setTabToolTip( target, toolTipStr );
}

void DictGroupsWidget::contextMenu( QPoint const & pos )
{
  const int clickedGroup = tabBar()->tabAt( pos );
  if( clickedGroup < 0 )
    return;
  const QString name = tabText( clickedGroup );
  if( name.length() != 7 || name.mid( 2, 3 ) != " - " )
    return;

  QMenu menu( this );

  const auto combineSourceAction =
    new QAction( QString( tr( "Combine groups by source language to \"%1->\"" ) ).arg( name.left( 2 ) ), &menu );
  combineSourceAction->setEnabled( false );

  QString grLeft = name.left( 2 );
  QString grRight = name.right( 2 );
  for( int i = 0; i < count(); i++ )
  {
    QString str = tabText( i );
    if( i != clickedGroup && str.length() == 7 && str.mid( 2, 3 ) == " - " && str.startsWith( grLeft ) )
    {
      combineSourceAction->setEnabled( true );
      break;
    }
  }
  menu.addAction( combineSourceAction );

  const auto combineTargetAction =
    new QAction( QString( tr( "Combine groups by target language to \"->%1\"" ) ).arg( name.right( 2 ) ), &menu );
  combineTargetAction->setEnabled( false );

  for( int i = 0; i < count(); i++ )
  {
    QString str = tabText( i );
    if( i != clickedGroup && str.length() == 7 && str.mid( 2, 3 ) == " - " && str.endsWith( grRight ) )
    {
      combineTargetAction->setEnabled( true );
      break;
    }
  }
  menu.addAction( combineTargetAction );

  QAction * combineTwoSidedAction = nullptr;
  if( grLeft != grRight )
  {
    combineTwoSidedAction =
      new QAction( QString( tr( "Make two-side translate group \"%1-%2-%1\"" ) ).arg( grLeft, grRight ), &menu );

    combineTwoSidedAction->setEnabled( false );

    const QString str = grRight + " - " + grLeft;
    for( int i = 0; i < count(); i++ )
    {
      if( str == tabText( i ) )
      {
        combineTwoSidedAction->setEnabled( true );
        break;
      }
    }

    menu.addAction( combineTwoSidedAction );
  }

  const auto combineFirstAction = new QAction( QString( tr( "Combine groups with \"%1\"" ) ).arg( grLeft ), &menu );
  combineFirstAction->setEnabled( false );
  for( int i = 0; i < count(); i++ )
  {
    QString str = tabText( i );
    if( i != clickedGroup && str.length() == 7 && str.mid( 2, 3 ) == " - " &&
        ( str.startsWith( grLeft ) || str.endsWith( grLeft ) ) )
    {
      combineFirstAction->setEnabled( true );
      break;
    }
  }
  menu.addAction( combineFirstAction );

  QAction * combineSecondAction = nullptr;

  if( grLeft != grRight )
  {
    combineSecondAction = new QAction( QString( tr( "Combine groups with \"%1\"" ) )
                                        .arg( grRight ), &menu );
    combineSecondAction->setEnabled( false );

    for( int i = 0; i < count(); i++ )
    {
      QString str = tabText( i );
      if( i != clickedGroup && str.length() == 7 && str.mid( 2, 3 ) == " - " &&
          ( str.startsWith( grRight ) || str.endsWith( grRight ) ) )
      {
        combineSecondAction->setEnabled( true );
        break;
      }
    }
    menu.addAction( combineSecondAction );
  }

  const QAction * result = menu.exec( mapToGlobal( pos ) );

  setUpdatesEnabled( false );
  int targetGroup;

  if ( result && result == combineSourceAction ) {
    targetGroup = addUniqueGroup( grLeft + "->" );

    for ( int i = 0; i < count(); i++ ) {
      QString str = tabText( i );
      if ( str.length() == 7 && str.mid( 2, 3 ) == " - " && str.startsWith( grLeft ) )
        combineGroups( i, targetGroup );
    }

    setCurrentIndex( targetGroup );
  }
  else if ( result && result == combineTargetAction ) {
    targetGroup = addUniqueGroup( "->" + grRight );

    for ( int i = 0; i < count(); i++ ) {
      QString str = tabText( i );
      if ( str.length() == 7 && str.mid( 2, 3 ) == " - " && str.endsWith( grRight ) )
        combineGroups( i, targetGroup );
    }

    setCurrentIndex( targetGroup );
  }
  else if ( result && result == combineTwoSidedAction ) {
    targetGroup       = addUniqueGroup( name + " - " + grLeft );
    const QString str = grRight + " - " + grLeft;

    for ( int i = 0; i < count(); i++ )
      if ( tabText( i ) == name || tabText( i ) == str )
        combineGroups( i, targetGroup );

    setCurrentIndex( targetGroup );
  }
  else if ( result && ( result == combineFirstAction || result == combineSecondAction ) ) {
    QString const & grBase = result == combineFirstAction ? grLeft : grRight;
    targetGroup = addUniqueGroup( grBase );

    for( int i = 0; i < count(); i++ )
    {
      QString str = tabText( i );
      if( str.length() == 7 && str.mid( 2, 3 ) == " - " &&
          ( str.startsWith( grBase ) || str.endsWith( grBase ) ) )
        combineGroups( i, targetGroup );
    }

    setCurrentIndex( targetGroup );
  }

  setUpdatesEnabled( true );
}

void DictGroupsWidget::tabDataChanged()
{
  const QString toolTipStr = "\"" + tabText( currentIndex() ) + "\"\n" + tr( "Dictionaries: " )
    + QString::number( getCurrentModel()->getCurrentDictionaries().size() );
  setTabToolTip( currentIndex(), toolTipStr );
}

QuickFilterLine::QuickFilterLine( QWidget * parent ): QLineEdit( parent ), m_focusAction(this)
{
  m_proxyModel.setFilterCaseSensitivity( Qt::CaseInsensitive );

  setPlaceholderText( tr( "Dictionary search/filter (Ctrl+F)" ) );

  m_focusAction.setShortcut( QKeySequence( "Ctrl+F" ) );
  connect( &m_focusAction, &QAction::triggered, this, &QuickFilterLine::focusFilterLine );

  QAction * clear = new QAction( QIcon(":/icons/clear.png"), tr("Clear Search"),this);
  connect(clear,&QAction::triggered,this, &QLineEdit::clear);

  addAction(clear,QLineEdit::TrailingPosition);
  addAction(new QAction(QIcon(":/icons/system-search.svg"),"",this),QLineEdit::LeadingPosition);

  setFocusPolicy(Qt::StrongFocus);

  connect( this, &QLineEdit::textChanged, this, &QuickFilterLine::filterChangedInternal );
}

QuickFilterLine::~QuickFilterLine()
{
}

void QuickFilterLine::applyTo( QAbstractItemView * source )
{
  m_source = source;
  m_proxyModel.setSourceModel( source->model() );
  source->setModel( &m_proxyModel );
}

QModelIndex QuickFilterLine::mapToSource( QModelIndex const & idx )
{
  if ( &m_proxyModel == idx.model() )
  {
    return m_proxyModel.mapToSource( idx );
  }
  else
  {
    return idx;
  }
}

void QuickFilterLine::filterChangedInternal()
{
  // emit signal in async manner, to avoid UI slowdown
  QTimer::singleShot( 1, this, &QuickFilterLine::emitFilterChanged );
}

void QuickFilterLine::emitFilterChanged()
{
  m_proxyModel.setFilterFixedString(text());
  emit filterChanged( text() );
}

void QuickFilterLine::focusFilterLine()
{
  setFocus();
  selectAll();
}

void QuickFilterLine::keyPressEvent( QKeyEvent * event )
{
  switch ( event->key() )
  {
    case Qt::Key_Down:
      if ( m_source )
      {
        m_source->setCurrentIndex( m_source->model()->index( 0,0 ) );
        m_source->setFocus();
      }
      break;
    default:
      QLineEdit::keyPressEvent( event );
  }
}
