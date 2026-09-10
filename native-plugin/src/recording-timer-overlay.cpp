#include "recording-timer-overlay.hpp"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QVBoxLayout>

#include <cmath>

#include <obs-module.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
constexpr auto kBackground = "rgba(12, 14, 18, 210)";
constexpr auto kBorder = "rgba(255, 255, 255, 36)";
constexpr auto kTextColor = "#F5F7FA";
constexpr auto kRecordingColor = "#3DFF8A";
constexpr auto kPausedColor = "#FFC857";
constexpr int kMoveClampPadding = 8;
constexpr double kPulsePeriodSeconds = 1.0;
constexpr double kPulseMinOpacity = 0.35;
constexpr double kPulseMaxOpacity = 1.0;

void excludeFromCapture(QWidget *widget) {
#ifdef _WIN32
  if (!widget) {
    return;
  }

  const WId wid = widget->winId();
  if (!wid) {
    return;
  }

  HWND hwnd = reinterpret_cast<HWND>(wid);
  SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
#else
  (void)widget;
#endif
}
} // namespace

RecordingTimerOverlay::RecordingTimerOverlay(QWidget *parent)
    : QWidget(parent), pillFrame(new QFrame(this)),
      iconLabel(new QLabel(QStringLiteral("\xE2\x97\x8F"), pillFrame)),
      timeLabel(new QLabel(QStringLiteral("00:00"), pillFrame)),
      updateTimer(new QTimer(this)),
      iconOpacity(new QGraphicsOpacityEffect(this)), hasSavedPosition(false),
      dragging(false), recording(false), paused(false), overlayShown(false),
      lastDisplayedSecond(-1),
      segmentStart(std::chrono::steady_clock::now()),
      pulseStart(std::chrono::steady_clock::now()), elapsedSeconds(0.0) {
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setCursor(Qt::SizeAllCursor);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);
  rootLayout->addWidget(pillFrame);

  pillFrame->setObjectName(QStringLiteral("timerPill"));
  pillFrame->setStyleSheet(QStringLiteral("#timerPill {"
                                          "  background-color: %1;"
                                          "  border: 1px solid %2;"
                                          "  border-radius: 16px;"
                                          "}")
                               .arg(kBackground, kBorder));

  auto *layout = new QHBoxLayout(pillFrame);
  layout->setContentsMargins(12, 7, 14, 7);
  layout->setSpacing(8);

  iconLabel->setFixedWidth(18);
  iconLabel->setAlignment(Qt::AlignCenter);
  iconLabel->setGraphicsEffect(iconOpacity);
  iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  pillFrame->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  timeLabel->setStyleSheet(
      QStringLiteral(
          "color:%1;"
          "font-size:18px;"
          "font-weight:600;"
          "letter-spacing:0.5px;"
          "background:transparent;"
          "border:none;"
          "font-family:'Cascadia Mono','Consolas','Courier New',monospace;")
          .arg(kTextColor));

  layout->addWidget(iconLabel);
  layout->addWidget(timeLabel);

  updateIcon();

  connect(updateTimer, &QTimer::timeout, this, &RecordingTimerOverlay::tick);
  // Timer so roda enquanto a gravacao esta ativa (ver setOverlayVisible).
  updateTimer->setInterval(50);

  hide();
}

void RecordingTimerOverlay::setConfigPath(const QString &path) {
  configPath = path;
}

void RecordingTimerOverlay::loadPosition(const QPoint &position, bool saved) {
  hasSavedPosition = saved;
  if (saved) {
    overlayPos = position;
    blog(LOG_INFO, "[obs-overlay-time-show] posicao carregada: %d,%d",
         position.x(), position.y());
  }
}

void RecordingTimerOverlay::savePosition() const {
  if (configPath.isEmpty()) {
    blog(LOG_WARNING,
         "[obs-overlay-time-show] caminho de config vazio; posicao nao salva");
    return;
  }

  if (!hasSavedPosition) {
    return;
  }

  const QFileInfo info(configPath);
  if (!QDir().mkpath(info.absolutePath())) {
    blog(LOG_WARNING,
         "[obs-overlay-time-show] nao foi possivel criar pasta: %s",
         info.absolutePath().toUtf8().constData());
    return;
  }

  QJsonObject obj;
  obj.insert(QStringLiteral("x"), overlayPos.x());
  obj.insert(QStringLiteral("y"), overlayPos.y());

  QFile file(configPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    blog(LOG_WARNING, "[obs-overlay-time-show] falha ao salvar posicao em %s",
         configPath.toUtf8().constData());
    return;
  }

  file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
  file.close();
  blog(LOG_INFO, "[obs-overlay-time-show] posicao salva: %d,%d -> %s",
       overlayPos.x(), overlayPos.y(), configPath.toUtf8().constData());
}

void RecordingTimerOverlay::clampToScreen() {
  QScreen *screen = QGuiApplication::screenAt(overlayPos);
  if (!screen)
    screen = QApplication::screenAt(QCursor::pos());
  if (!screen)
    screen = QApplication::primaryScreen();
  if (!screen) {
    return;
  }

  adjustSize();
  const QRect area = screen->availableGeometry();
  const int maxX = area.right() - width() - kMoveClampPadding;
  const int maxY = area.bottom() - height() - kMoveClampPadding;

  overlayPos.setX(
      qBound(area.left() + kMoveClampPadding, overlayPos.x(), maxX));
  overlayPos.setY(qBound(area.top() + kMoveClampPadding, overlayPos.y(), maxY));
}

void RecordingTimerOverlay::moveBy(int dx, int dy) {
  if (!hasSavedPosition) {
    overlayPos = isVisible() ? pos() : overlayPos;
    hasSavedPosition = true;
  }

  overlayPos += QPoint(dx, dy);
  clampToScreen();
  move(overlayPos);
  savePosition();

  if (isVisible()) {
    raise();
    applyExcludeFromCapture();
  }
}

void RecordingTimerOverlay::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }

  dragging = true;
  dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
  event->accept();
}

void RecordingTimerOverlay::mouseMoveEvent(QMouseEvent *event) {
  if (!dragging || !(event->buttons() & Qt::LeftButton)) {
    return;
  }

  overlayPos = event->globalPosition().toPoint() - dragOffset;
  hasSavedPosition = true;
  clampToScreen();
  move(overlayPos);
  raise();
  applyExcludeFromCapture();
  event->accept();
}

void RecordingTimerOverlay::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton || !dragging) {
    return;
  }

  dragging = false;
  clampToScreen();
  move(overlayPos);
  savePosition();
  applyExcludeFromCapture();
  event->accept();
}

void RecordingTimerOverlay::applyExcludeFromCapture() {
  excludeFromCapture(this);
}

void RecordingTimerOverlay::updateIcon() {
  if (paused) {
    iconLabel->setText(QStringLiteral("\xE2\x8F\xB8"));
    iconLabel->setStyleSheet(
        QStringLiteral("color:%1;font-size:15px;font-weight:bold;background:"
                       "transparent;border:none;")
            .arg(kPausedColor));
    iconOpacity->setOpacity(1.0);
    return;
  }

  iconLabel->setText(QStringLiteral("\xE2\x97\x8F"));
  iconLabel->setStyleSheet(
      QStringLiteral("color:%1;font-size:13px;font-weight:bold;background:"
                     "transparent;border:none;")
          .arg(kRecordingColor));
}

void RecordingTimerOverlay::updatePulse() {
  if (!recording || paused) {
    iconOpacity->setOpacity(1.0);
    return;
  }

  const std::chrono::duration<double> elapsed =
      std::chrono::steady_clock::now() - pulseStart;
  const double phase =
      std::fmod(elapsed.count(), kPulsePeriodSeconds) / kPulsePeriodSeconds;
  const double wave =
      0.5 + 0.5 * std::sin(phase * 2.0 * 3.14159265358979323846);
  const double opacity =
      kPulseMinOpacity + (kPulseMaxOpacity - kPulseMinOpacity) * wave;
  iconOpacity->setOpacity(opacity);
}

void RecordingTimerOverlay::setOverlayVisible(bool visible) {
  if (visible == overlayShown && visible == isVisible()) {
    return;
  }

  if (visible) {
    if (!hasSavedPosition) {
      const QScreen *screen = QApplication::primaryScreen();
      if (screen) {
        adjustSize();
        const QRect area = screen->availableGeometry();
        overlayPos = QPoint(area.right() - width() - 24, area.top() + 24);
        hasSavedPosition = true;
        savePosition();
      }
    }

    clampToScreen();
    move(overlayPos);
    show();
    raise();
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#endif
    applyExcludeFromCapture();
    if (!updateTimer->isActive())
      updateTimer->start();
    overlayShown = true;
  } else {
    hide();
    updateTimer->stop();
    overlayShown = false;
  }
}

QString RecordingTimerOverlay::formatElapsed() const {
  const int total = static_cast<int>(elapsedSeconds);
  const int hours = total / 3600;
  const int minutes = (total % 3600) / 60;
  const int seconds = total % 60;

  if (hours > 0) {
    return QStringLiteral("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
  }

  return QStringLiteral("%1:%2")
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'));
}

void RecordingTimerOverlay::tick() {
  const auto now = std::chrono::steady_clock::now();

  if (recording && !paused) {
    const std::chrono::duration<double> delta = now - segmentStart;
    elapsedSeconds += delta.count();
    segmentStart = now;
  } else if (recording && paused) {
    segmentStart = now;
  }

  const int displayed = static_cast<int>(elapsedSeconds);
  if (displayed != lastDisplayedSecond) {
    timeLabel->setText(formatElapsed());
    lastDisplayedSecond = displayed;
  }
  updateIcon();
  updatePulse();
  setOverlayVisible(recording);
}

void RecordingTimerOverlay::onRecordingStarted(double seededSeconds) {
  recording = true;
  paused = false;
  elapsedSeconds = seededSeconds > 0.0 ? seededSeconds : 0.0;
  lastDisplayedSecond = -1;
  segmentStart = std::chrono::steady_clock::now();
  pulseStart = segmentStart;
  tick();
}

void RecordingTimerOverlay::onRecordingStopped() {
  recording = false;
  paused = false;
  elapsedSeconds = 0.0;
  tick();
}

void RecordingTimerOverlay::onRecordingPaused() {
  if (!recording) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  const std::chrono::duration<double> delta = now - segmentStart;
  elapsedSeconds += delta.count();
  paused = true;
  segmentStart = now;
  tick();
}

void RecordingTimerOverlay::onRecordingUnpaused() {
  if (!recording) {
    return;
  }

  paused = false;
  segmentStart = std::chrono::steady_clock::now();
  pulseStart = segmentStart;
  tick();
}
