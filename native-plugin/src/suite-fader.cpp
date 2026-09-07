#include "suite-fader.hpp"
#include "suite-actions.hpp"

#include <obs-module.h>
#include <obs.h>

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <cmath>

namespace SuiteFader {
namespace {

constexpr int kTickMs = 25;

double clamp01(double value)
{
  if (value < 0.0)
    return 0.0;
  if (value > 1.0)
    return 1.0;
  return value;
}

// O OBS usa um fader cubico: a posicao do controle elevada ao cubo vira volume.
// Interpolar a posicao (e nao o volume) deixa o fade parecido com arrastar o
// controle de volume na mao, sem saltos no fim.
double volumeToPosition(double volume)
{
  return std::cbrt(clamp01(volume));
}

double positionToVolume(double position)
{
  const double clamped = clamp01(position);
  return clamp01(clamped * clamped * clamped);
}

struct ActiveFade {
  obs_weak_source_t *weak = nullptr;
  QString sceneName;
  QString sourceName;
  double startPosition = 0.0;
  double endPosition = 0.0;
  double targetVolume = 0.0;
  double restoreVolume = 1.0;
  int durationMs = 0;
  bool hideItemWhenDone = false;
  bool restoreVolumeWhenDone = false;
  QElapsedTimer clock;
};

class Fader : public QObject {
public:
  Fader()
  {
    m_timer.setInterval(kTickMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this]() { tick(); });
  }

  void add(ActiveFade fade)
  {
    dropExisting(fade.sourceName);
    fade.clock.start();
    const bool immediate = fade.durationMs <= 0;
    m_fades.append(fade);
    if (immediate) {
      tick();
      return;
    }
    if (!m_timer.isActive())
      m_timer.start();
  }

  void finishAll(bool applyTarget)
  {
    m_timer.stop();
    QVector<ActiveFade> fades;
    fades.swap(m_fades);
    for (ActiveFade &fade : fades)
      finalize(fade, applyTarget);
  }

private:
  void dropExisting(const QString &sourceName)
  {
    for (int i = m_fades.size() - 1; i >= 0; --i) {
      if (m_fades.at(i).sourceName != sourceName)
        continue;
      ActiveFade fade = m_fades.at(i);
      m_fades.removeAt(i);
      finalize(fade, false);
    }
  }

  void finalize(ActiveFade &fade, bool applyTarget)
  {
    obs_source_t *source =
        fade.weak ? obs_weak_source_get_source(fade.weak) : nullptr;
    if (source) {
      if (applyTarget)
        obs_source_set_volume(source, static_cast<float>(fade.targetVolume));
      obs_source_release(source);
    }

    if (applyTarget && fade.hideItemWhenDone)
      SuiteActions::setSourceVisible(fade.sceneName, fade.sourceName, false);

    if (applyTarget && fade.restoreVolumeWhenDone) {
      obs_source_t *again =
          fade.weak ? obs_weak_source_get_source(fade.weak) : nullptr;
      if (again) {
        obs_source_set_volume(again, static_cast<float>(fade.restoreVolume));
        obs_source_release(again);
      }
    }

    if (fade.weak) {
      obs_weak_source_release(fade.weak);
      fade.weak = nullptr;
    }
  }

  void tick()
  {
    for (int i = m_fades.size() - 1; i >= 0; --i) {
      ActiveFade &fade = m_fades[i];

      obs_source_t *source =
          fade.weak ? obs_weak_source_get_source(fade.weak) : nullptr;
      if (!source) {
        ActiveFade dead = fade;
        m_fades.removeAt(i);
        finalize(dead, false);
        continue;
      }

      const double elapsed = static_cast<double>(fade.clock.elapsed());
      const double progress =
          fade.durationMs > 0 ? clamp01(elapsed / fade.durationMs) : 1.0;
      const double position =
          fade.startPosition + (fade.endPosition - fade.startPosition) * progress;
      obs_source_set_volume(source, static_cast<float>(positionToVolume(position)));
      obs_source_release(source);

      if (progress >= 1.0) {
        ActiveFade done = fade;
        m_fades.removeAt(i);
        finalize(done, true);
      }
    }

    if (m_fades.isEmpty())
      m_timer.stop();
  }

  QTimer m_timer;
  QVector<ActiveFade> m_fades;
};

Fader *fader()
{
  // Intencionalmente sem destruir: evita mexer em QTimer no fim do processo.
  static Fader *instance = new Fader();
  return instance;
}

} // namespace

bool startFade(const FadeRequest &request)
{
  if (request.sourceName.trimmed().isEmpty())
    return false;

  obs_source_t *source =
      obs_get_source_by_name(request.sourceName.toUtf8().constData());
  if (!source)
    return false;

  const double currentVolume = clamp01(obs_source_get_volume(source));
  const double startVolume =
      request.startVolume >= 0.0 ? clamp01(request.startVolume) : currentVolume;

  ActiveFade fade;
  fade.sceneName = request.sceneName;
  fade.sourceName = request.sourceName;
  fade.startPosition = volumeToPosition(startVolume);
  fade.endPosition = volumeToPosition(request.targetVolume);
  fade.targetVolume = clamp01(request.targetVolume);
  fade.restoreVolume = currentVolume;
  fade.durationMs = request.durationMs > 0 ? request.durationMs : 0;
  fade.hideItemWhenDone = request.hideItemWhenDone;
  fade.restoreVolumeWhenDone = request.restoreVolumeWhenDone;
  fade.weak = obs_source_get_weak_source(source);

  obs_source_set_volume(source, static_cast<float>(startVolume));
  if (request.unmuteAtStart)
    obs_source_set_muted(source, false);
  obs_source_release(source);

  fader()->add(fade);
  return true;
}

void finishAllNow()
{
  fader()->finishAll(true);
}

void cancelAll()
{
  fader()->finishAll(false);
}

} // namespace SuiteFader
