#pragma once

#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>
#include <QWidget>

#include <chrono>

class QGraphicsOpacityEffect;

class RecordingTimerOverlay : public QWidget {
  Q_OBJECT

public:
  explicit RecordingTimerOverlay(QWidget *parent = nullptr);

  void loadPosition(const QPoint &position, bool hasSavedPosition);
  void savePosition() const;
  void setConfigPath(const QString &path);

  void moveBy(int dx, int dy);

  void onRecordingStarted(double seededSeconds = 0.0);
  void onRecordingStopped();
  void onRecordingPaused();
  void onRecordingUnpaused();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
  void tick();

private:
  void applyExcludeFromCapture();
  void clampToScreen();
  void updateIcon();
  void updatePulse();
  void setOverlayVisible(bool visible);
  QString formatElapsed() const;

  QFrame *pillFrame;
  QLabel *iconLabel;
  QLabel *timeLabel;
  QTimer *updateTimer;
  QGraphicsOpacityEffect *iconOpacity;

  QString configPath;
  QPoint overlayPos;
  bool hasSavedPosition;

  bool dragging;
  QPoint dragOffset;

  bool recording;
  bool paused;
  bool overlayShown;
  int lastDisplayedSecond;
  std::chrono::steady_clock::time_point segmentStart;
  std::chrono::steady_clock::time_point pulseStart;
  double elapsedSeconds;
};
