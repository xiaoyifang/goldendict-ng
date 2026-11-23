/* This file is part of GoldenDict.
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include <QObject>
#include <QSize>
#include <QTimer>
#include <functional>

class QEvent;
class QScreen;
class QWidget;

/// Keeps a window's user-chosen size stable across display topology changes.
///
/// The size the user picked is remembered separately from the size the window
/// may temporarily be forced to by the window system, so unplugging a monitor
/// no longer shrinks the window for good.
class WindowGeometryKeeper final : public QObject
{
public:
  /// Adjusts the desired size for the target screen, e.g. to apply window
  /// specific constraints. Screen may be null when no screen is available.
  using SizeConstraint = std::function< QSize( const QSize & desiredSize, QScreen * screen ) >;

  /// Called after the size has been restored, e.g. to clamp the position.
  using PostRestore = std::function< void( QScreen * screen ) >;

  explicit WindowGeometryKeeper( QWidget * window, SizeConstraint sizeConstraint = {}, PostRestore postRestore = {} );

  bool isHandlingScreenChange() const
  {
    return handlingScreenChange;
  }

  /// Restores the last stable size right away if a screen change is pending,
  /// so that intermediate sizes are never persisted.
  void prepareForSave();

  bool eventFilter( QObject * watched, QEvent * event ) override;

private:
  void watchScreen( QScreen * screen );
  void connectWindowScreenChange();
  void rememberNormalSize();
  void commitNormalSize();
  void scheduleScreenGeometryRestore();
  void restoreNormalSizeAfterScreenChange();

  QWidget * window;
  SizeConstraint sizeConstraint;
  PostRestore postRestore;
  QSize lastStableNormalSize;
  QSize pendingNormalSize;
  QTimer normalSizeTimer;
  QTimer screenChangeTimer;
  bool handlingScreenChange = false;
};
