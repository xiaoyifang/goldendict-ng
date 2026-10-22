#include "dictionarybar.hh"
#include "instances.hh"
#include "globalbroadcaster.hh"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QIconEngine>
#include <QMenu>
#include <QInputDialog>
#include <QPainter>
#include <QStringList>
#include "metadata.hh"
#include "common/utils.hh"
#include <QContextMenuEvent>
#include <QStyle>


using std::vector;

namespace {

/// A QIconEngine that paints another icon as a fixed grayscale image with a
/// strike-through line. Unlike QIcon::Disabled (which re-involves the Qt
/// style system and can change on hover / state changes), this engine ignores
/// the incoming Mode/State, so the icon looks the same grayed-out "disabled"
/// snapshot at all times.
///
/// The engine overrides pixmap() instead of paint(): all composition happens
/// on an offscreen image with a real alpha channel, and the returned pixmap
/// is blended onto widgets by Qt's regular icon painting path. This avoids
/// both stale-pixel artifacts (the target of paint() is not guaranteed to be
/// cleared) and the opaque-black result of clearing a widget's alpha-less
/// backing store.
class DimmedIconEngine: public QIconEngine
{
public:
  explicit DimmedIconEngine( const QIcon & source_ ): source( source_ ) {}

  QIconEngine * clone() const override
  {
    return new DimmedIconEngine( source );
  }

  void paint( QPainter * painter, const QRect & rect, QIcon::Mode mode, QIcon::State state ) override
  {
    painter->drawPixmap( rect, pixmap( rect.size(), mode, state ) );
  }

  QPixmap pixmap( const QSize & size, QIcon::Mode, QIcon::State ) override
  {
    if ( size.isEmpty() ) {
      return {};
    }

    // The source pixmap keeps aspect ratio, so it may be smaller than the
    // requested size (e.g. a 12x24 image fitted into a 24x24 request).
    QPixmap srcPixmap = source.pixmap( size, QIcon::Normal, QIcon::Off );
    if ( srcPixmap.isNull() ) {
      return srcPixmap;
    }

    // Convert to grayscale while keeping the alpha channel. The image is
    // unpremultiplied first: qGray() on premultiplied pixels would darken
    // semi-transparent edges.
    QImage srcImage = srcPixmap.toImage().convertToFormat( QImage::Format_ARGB32 );
    QImage grayImage( srcImage.size(), QImage::Format_ARGB32 );
    for ( int y = 0; y < srcImage.height(); ++y ) {
      const QRgb * srcLine = reinterpret_cast< const QRgb * >( srcImage.constScanLine( y ) );
      QRgb * dstLine       = reinterpret_cast< QRgb * >( grayImage.scanLine( y ) );
      for ( int x = 0; x < srcImage.width(); ++x ) {
        const QRgb rgba  = srcLine[ x ];
        const int gValue = qGray( rgba );
        dstLine[ x ]     = qRgba( gValue, gValue, gValue, qAlpha( rgba ) );
      }
    }

    // Switch back to premultiplied alpha. If an unpremultiplied image is
    // scaled (SmoothPixmapTransform), bilinear interpolation on zero-alpha
    // pixels produces black contamination along translucent edges.
    grayImage = grayImage.convertToFormat( QImage::Format_ARGB32_Premultiplied );

    // Always compose on a canvas of the requested logical size. The source
    // image is centered inside it, and the strike-through line spans the
    // whole canvas, so the line has the same length for every icon
    // regardless of the source image's aspect ratio.
    const qreal dpr = qMax< qreal >( srcPixmap.devicePixelRatio(), 1.0 );
    QImage canvas( size * dpr, QImage::Format_ARGB32_Premultiplied );
    canvas.setDevicePixelRatio( dpr );
    canvas.fill( Qt::transparent );

    QPainter p( &canvas );
    p.setRenderHint( QPainter::Antialiasing, true );
    p.setRenderHint( QPainter::SmoothPixmapTransform, true );

    // Center the grayscale source. The explicit target rect (in logical
    // coordinates) determines geometry, so grayImage needs no DPR metadata.
    const QSizeF srcLogical = QSizeF( srcImage.size() ) / dpr;
    const QRectF target( ( size.width() - srcLogical.width() ) / 2.0,
                         ( size.height() - srcLogical.height() ) / 2.0,
                         srcLogical.width(),
                         srcLogical.height() );
    p.drawImage( target, grayImage );

    // Strike-through line across the full canvas. A bright color is used so
    // it stays visible on the dark grayscale icon.
    const qreal lineWidth = qMax< qreal >( 2, size.height() / 10.0 );
    p.setPen( QPen( QColor( 235, 90, 70, 230 ), lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin ) );
    const qreal padding = lineWidth + 2;
    p.drawLine( QPointF( padding, size.height() - padding ), QPointF( size.width() - padding, padding ) );
    p.end();

    return QPixmap::fromImage( canvas );
  }

private:
  QIcon source;
};

/// Returns a fixed-grayscale copy of @p source. A null icon is returned
/// unchanged so text-only dictionary entries are not affected.
QIcon makeDimmedIcon( const QIcon & source )
{
  if ( source.isNull() ) {
    return source;
  }
  return QIcon( new DimmedIconEngine( source ) );
}

/// Returns the path of the marker file which schedules one of the dictionary
/// indices for a rebuild. Dictionary::needToRebuildIndex() consumes the marker
/// on the next application start. The FTS index lives in a separate file named
/// "<id>_FTS_x", so it needs its own marker.
QString reindexMarkerPath( const QString & dictId, bool fts )
{
  QString indexFileName = dictId;
  if ( fts ) {
    indexFileName += QString::fromStdString( Dictionary::getFtsSuffix() );
  }
  return Config::getIndexDir() + indexFileName + QStringLiteral( ".stale" );
}

/// True if the dictionary has already been scheduled for reindexing. Only the
/// headword index marker is checked, since it is created for every local
/// dictionary, unlike the optional FTS one.
bool hasReindexMarker( const QString & dictId )
{
  try {
    return QFileInfo::exists( reindexMarkerPath( dictId, false ) );
  }
  catch ( const std::exception & e ) {
    qWarning() << "Cannot access the index directory:" << e.what();
    return false;
  }
}
} // namespace

DictionaryBar::DictionaryBar( QWidget * parent, const unsigned short & maxDictionaryRefsInContextMenu_ ):
  QToolBar( tr( "&Dictionary Bar" ), parent ),
  mutedDictionaries( nullptr ),
  maxDictionaryRefsInContextMenu( maxDictionaryRefsInContextMenu_ )
{
  normalIconSize = { this->iconSize().height(), this->iconSize().height() };

  setObjectName( "dictionaryBar" );

  maxDictionaryRefsAction =
    new QAction( QIcon( ":/icons/addtab.svg" ), tr( "Extended menu with all dictionaries..." ), this );

  connect( GlobalBroadcaster::instance(),
           &GlobalBroadcaster::mutedDictionariesChanged,
           this,
           &DictionaryBar::mutedDictionariesChanged );

  connect( this, &QToolBar::actionTriggered, this, &DictionaryBar::actionWasTriggered );
}

static QString elideDictName( const QString & name )
{
  // Some names are way too long -- we insert an ellipsis in the middle of those

  const int maxSize = 33;

  if ( name.size() <= maxSize ) {
    return name;
  }

  const int pieceSize = maxSize / 2 - 1;

  return name.left( pieceSize ) + QChar( 0x2026 ) + name.right( pieceSize );
}

void DictionaryBar::setDictionaries( const vector< sptr< Dictionary::Class > > & dictionaries )
{
  setUpdatesEnabled( false );

  allDictionaries.clear();
  allDictionaries = dictionaries;

  clear();
  dictActions.clear();
  fullIcons.clear();
  dimmedIcons.clear();

  for ( const auto & dictionary : dictionaries ) {
    QIcon icon = dictionary->getIcon();

    QString dictName = QString::fromUtf8( dictionary->getName().c_str() );

    QAction * action = addAction( icon, elideDictName( dictName ) );

    action->setToolTip( dictName ); // Tooltip need not be shortened

    QString id = QString::fromStdString( dictionary->getId() );

    action->setData( id );

    action->setCheckable( true );

    action->setChecked( mutedDictionaries ? !mutedDictionaries->contains( id ) : true );

    // Keep two icon variants and switch them by checked state. The unchecked
    // variant is alpha-blended at reduced opacity, which fades naturally into
    // both light and dark backgrounds without relying on QSS.
    fullIcons.insert( action, icon );
    dimmedIcons.insert( action, makeDimmedIcon( icon ) );

    connect( action, &QAction::toggled, this, [ this, action ]( bool checked ) {
      applyActionIcon( action, checked );
    } );

    applyActionIcon( action, action->isChecked() );

    dictActions.append( action );
  }


  setUpdatesEnabled( true );
}

void DictionaryBar::updateToGroup( const Instances::Group * grp,
                                   QSet< QString > * allGroupMutedDictionaries,
                                   Config::Class & cfg,
                                   bool isPopup )
{
  Q_ASSERT( grp != nullptr ); // should never occur
  if ( !this->toggleViewAction()->isChecked() ) {
    return; // It's not enabled, therefore hidden -- don't waste time
  }

  setMutedDictionaries( nullptr );
  if ( grp->id == GroupId::AllGroupId ) {
    setMutedDictionaries( allGroupMutedDictionaries );
  }
  else {
    Config::Group * _grp = cfg.getGroup( grp->id );
    if ( _grp ) {
      setMutedDictionaries( isPopup ? &_grp->popupMutedDictionaries : &_grp->mutedDictionaries );
    }
  }
  setDictionaries( grp->dictionaries );
}

void DictionaryBar::setDictionaryIconSize( IconSize size )
{
  switch ( size ) {
    case IconSize::Small: {
      auto smallSize = QApplication::style()->pixelMetric( QStyle::PM_SmallIconSize );
      setIconSize( { smallSize, smallSize } );
      break;
    }

    case IconSize::Normal: {
      setIconSize( normalIconSize );
      break;
    }

    case IconSize::Large: {
      auto largeSize = QApplication::style()->pixelMetric( QStyle::PM_LargeIconSize );
      setIconSize( { largeSize + 10, largeSize + 10 } ); // the value isn't large enough, so we add 10
      break;
    }
  }
}

void DictionaryBar::contextMenuEvent( QContextMenuEvent * event )
{
  showContextMenu( event );
}


void DictionaryBar::showContextMenu( QContextMenuEvent * event, bool extended )
{
  QMenu menu( this );

  const QAction * restoreSelectionAction = nullptr;
  if ( tempSelectionCapturedMuted.has_value() ) {
    restoreSelectionAction = menu.addAction( tr( "Restore selection" ) );
  }

  const QAction * editAction            = nullptr;
  const QAction * infoAction            = nullptr;
  const QAction * headwordsAction       = nullptr;
  Dictionary::Class * pDict             = nullptr;
  const QAction * changeNameAction      = nullptr;
  const QAction * openDictFolderAction  = nullptr;
  const QAction * scheduleReindexAction = nullptr;

  editAction = menu.addAction( QIcon( ":/icons/bookcase.svg" ), tr( "Edit this group" ) );

  QString dictFilename;

  QAction * dictAction = actionAt( event->x(), event->y() );
  if ( dictAction ) {
    const QString id          = dictAction->data().toString();
    for ( const auto & dictionary : allDictionaries ) {
      if ( id.compare( dictionary->getId().c_str() ) == 0 ) {
        pDict = dictionary.get();
        break;
      }
    }

    if ( pDict ) {
      infoAction = menu.addAction( tr( "Dictionary info" ) );

      // All actions below are only for local dictionaries
      if ( pDict->isLocalDictionary() ) {
        changeNameAction = menu.addAction( tr( "Change display name" ) );

        // Add schedule/cancel reindex action
        const QString dictId = QString::fromStdString( pDict->getId() );
        if ( hasReindexMarker( dictId ) ) {
          scheduleReindexAction = menu.addAction( tr( "Cancel reindex" ) );
        }
        else {
          scheduleReindexAction = menu.addAction( tr( "Schedule for reindex" ) );
        }

        // Add dictionary headwords action
        if ( pDict->getWordCount() > 0 ) {
          headwordsAction = menu.addAction( tr( "Dictionary headwords" ) );
        }
        openDictFolderAction = menu.addAction( tr( "Open dictionary folder" ) );
      }
    }
  }

  if ( !dictActions.empty() ) {
    menu.addSeparator();
  }

  unsigned refsAdded = 0;

  for ( const auto & dictAction : std::as_const( dictActions ) ) {

    // Enough! Or the menu would become too large.
    if ( refsAdded++ >= maxDictionaryRefsInContextMenu && !extended ) {
      menu.addSeparator();
      menu.addAction( maxDictionaryRefsAction );
      break;
    }

    // We need new action, since the one we have has text elided
    QAction * action = menu.addAction( dictAction->icon(), dictAction->toolTip() );

    action->setCheckable( true );
    action->setChecked( dictAction->isChecked() );
    action->setData( QVariant::fromValue( (void *)dictAction ) );
    // Force "icon in menu" on all platforms, for
    // usability reasons.
    action->setIconVisibleInMenu( true );
  }

  connect( this, &DictionaryBar::closePopupMenu, &menu, &QWidget::close );

  const QAction * result = menu.exec( event->globalPos() );

  if ( result && result == infoAction ) {
    const QString id = dictAction->data().toString();
    emit showDictionaryInfo( id );
    return;
  }

  if ( result && result == changeNameAction ) {
    if ( !pDict || pDict->getContainingFolder().isEmpty() ) {
      return;
    }
    QInputDialog dialog( this );
    dialog.setWindowTitle( tr( "Change display name" ) );
    dialog.setLabelText( tr( "New display name:" ) );
    dialog.setTextValue( QString::fromUtf8( pDict->getName().c_str() ) );
    if ( auto * lineEdit = dialog.findChild< QLineEdit * >() ) {
      lineEdit->setClearButtonEnabled( true );
    }

    if ( dialog.exec() == QDialog::Accepted ) {
      QString newName = dialog.textValue();
      Metadata::saveDisplayName( Utils::Path::combine( pDict->getContainingFolder(), "metadata.toml" ).toStdString(),
                                 newName.toStdString() );
      pDict->setName( newName.toStdString() );
      QString effectiveName = QString::fromUtf8( pDict->getName().c_str() );
      dictAction->setText( elideDictName( effectiveName ) );
      dictAction->setToolTip( effectiveName );
    }
  }

  if ( result && result == scheduleReindexAction ) {
    const QString dictId = dictAction->data().toString();

    try {
      // An empty "<index file>.stale" marker next to the index file makes
      // Dictionary::needToRebuildIndex() rebuild that index on the next start.
      QStringList markers{ reindexMarkerPath( dictId, false ) };
      if ( pDict && pDict->canFTS() ) {
        markers.append( reindexMarkerPath( dictId, true ) );
      }

      if ( QFileInfo::exists( markers.first() ) ) {
        // Remove the markers to cancel the scheduled reindex
        for ( const auto & marker : std::as_const( markers ) ) {
          QFile::remove( marker );
        }

        // Note: Using status bar notification instead of message box for lighter user feedback
        emit showStatusBarMessage( tr( "Cancel schedule reindex" ), 3000 ); // Show for 3 seconds
      }
      else {
        bool success = true;
        for ( const auto & marker : std::as_const( markers ) ) {
          QFile markerFile( marker );
          success = markerFile.open( QIODevice::WriteOnly ) && success;
          markerFile.close();
        }

        if ( success ) {
          // Using status bar notification instead of message box for lighter user feedback
          emit showStatusBarMessage(
            tr(
              "The dictionary has been scheduled for reindexing. The index will be rebuilt on the next application restart." ),
            3000 ); // Show for 3 seconds
        }
        else {
          emit showStatusBarMessage( tr( "Failed to schedule the dictionary for reindexing." ), 3000 );
        }
      }
    }
    catch ( const std::exception & e ) {
      qWarning() << "Cannot write the reindex marker:" << e.what();
      emit showStatusBarMessage( tr( "Failed to schedule the dictionary for reindexing." ), 3000 );
    }

    return;
  }

  if ( result && result == headwordsAction ) {
    const std::string id = dictAction->data().toString().toStdString();
    for ( const auto & dict : allDictionaries ) {
      if ( id == dict->getId() ) {
        emit showDictionaryHeadwords( dict.get() );
        break;
      }
    }
    return;
  }

  if ( result && result == openDictFolderAction ) {
    const QString id = dictAction->data().toString();
    emit openDictionaryFolder( id );
    return;
  }

  if ( result && result == maxDictionaryRefsAction ) {
    showContextMenu( event, true );
  }

  if ( result && result == restoreSelectionAction ) {
    *mutedDictionaries = tempSelectionCapturedMuted.value();
    tempSelectionCapturedMuted.reset();
    GlobalBroadcaster::instance()->mutedDictionariesChanged();
  }

  if ( result == editAction ) {
    emit editGroupRequested();
  }
  else if ( result && result->data().value< void * >() ) {
    ( (QAction *)( result->data().value< void * >() ) )->trigger();
  }

  event->accept();
}

void DictionaryBar::mutedDictionariesChanged()
{
  //qDebug( "Muted dictionaries changed" );

  if ( !mutedDictionaries ) {
    return;
  }

  // Update actions

  setUpdatesEnabled( false );

  for ( const auto & dictAction : std::as_const( dictActions ) ) {
    const bool isUnmuted = !mutedDictionaries->contains( dictAction->data().toString() );

    if ( isUnmuted != dictAction->isChecked() ) {
      dictAction->setChecked( isUnmuted );
    }
  }

  setUpdatesEnabled( true );
}

void DictionaryBar::actionWasTriggered( QAction * action )
{
  if ( !mutedDictionaries ) {
    return;
  }

  const QString id = action->data().toString();

  if ( id.isEmpty() ) {
    return; // Some weird action, not our button
  }

  //  Ctrl Click Single Selection
  if ( QApplication::keyboardModifiers().testFlag( Qt::ControlModifier ) ) {
    // Ctrl+Clicked the only one selected
    if ( ( dictActions.size() - mutedDictionaries->size() ) == 1 && !action->isChecked() ) {
      mutedDictionaries->clear();
    }
    else {
      selectSingleDict( id );
    }
  }
  ///  Shift Click Capturing
  else if ( QApplication::keyboardModifiers().testFlag( Qt::ShiftModifier ) ) {
    tempSelectionCapturedMuted.emplace( *mutedDictionaries );
    selectSingleDict( id ); // Give user feedback that capturing success by select the one clicked
  }
  else { // Normal clicking
    if ( action->isChecked() ) {
      mutedDictionaries->remove( id );
    }
    else {
      mutedDictionaries->insert( id );
    }
  }
  GlobalBroadcaster::instance()->mutedDictionariesChanged();
}

void DictionaryBar::selectSingleDict( const QString & id )
{
  for ( auto & dictAction : std::as_const( dictActions ) ) {
    const QString dictId = dictAction->data().toString();
    if ( dictId == id ) {
      mutedDictionaries->remove( dictId );
    }
    else {
      mutedDictionaries->insert( dictId );
    }
  }
}

void DictionaryBar::dictsPaneClicked( const QString & id )
{
  if ( !isVisible() ) {
    return;
  }

  for ( const auto & dictAction : std::as_const( dictActions ) ) {
    const QString dictId = dictAction->data().toString();
    if ( dictId == id ) {
      dictAction->activate( QAction::Trigger );
      break;
    }
  }
}

void DictionaryBar::applyActionIcon( QAction * action, bool checked )
{
  if ( action == nullptr ) {
    return;
  }

  const auto it = fullIcons.constFind( action );
  if ( it == fullIcons.constEnd() ) {
    return; // Not one of our dictionary actions
  }

  action->setIcon( checked ? it.value() : dimmedIcons.value( action ) );
}
