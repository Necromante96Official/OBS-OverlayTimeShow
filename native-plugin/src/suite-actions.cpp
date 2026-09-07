#include "suite-actions.hpp"
#include "suite-fader.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

namespace SuiteActions {
namespace {

obs_source_t *findSourceByName(const QString &name)
{
  if (name.isEmpty())
    return nullptr;
  return obs_get_source_by_name(name.toUtf8().constData());
}

} // namespace

QStringList sceneNames()
{
  QStringList names;
  char **list = obs_frontend_get_scene_names();
  if (!list)
    return names;
  for (char **it = list; *it; ++it)
    names.append(QString::fromUtf8(*it));
  bfree(list);
  return names;
}

QStringList sourceNamesInScene(const QString &sceneName)
{
  QStringList names;
  obs_source_t *sceneSource = findSourceByName(sceneName);
  if (!sceneSource)
    return names;

  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) {
    obs_source_release(sceneSource);
    return names;
  }

  auto cb = [](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
    auto *out = static_cast<QStringList *>(param);
    obs_source_t *src = obs_sceneitem_get_source(item);
    if (src)
      out->append(QString::fromUtf8(obs_source_get_name(src)));
    return true;
  };
  obs_scene_enum_items(scene, cb, &names);
  obs_source_release(sceneSource);
  return names;
}

QStringList transitionNames()
{
  QStringList names;
  obs_frontend_source_list list = {};
  obs_frontend_get_transitions(&list);
  for (size_t i = 0; i < list.sources.num; ++i) {
    obs_source_t *src = list.sources.array[i];
    if (src)
      names.append(QString::fromUtf8(obs_source_get_name(src)));
  }
  obs_frontend_source_list_free(&list);
  return names;
}

QStringList audioSourceNames()
{
  QStringList names;
  auto enumCb = [](void *param, obs_source_t *source) -> bool {
    auto *out = static_cast<QStringList *>(param);
    uint32_t flags = obs_source_get_output_flags(source);
    if (flags & OBS_SOURCE_AUDIO)
      out->append(QString::fromUtf8(obs_source_get_name(source)));
    return true;
  };
  obs_enum_sources(enumCb, &names);
  return names;
}

QVector<QPair<QString, QString>> itemTransitionTypes()
{
  QVector<QPair<QString, QString>> types;
  const char *id = nullptr;
  for (size_t i = 0; obs_enum_transition_types(i, &id); ++i) {
    if (!id)
      continue;
    const char *label = obs_source_get_display_name(id);
    types.append({QString::fromUtf8(id),
                  QString::fromUtf8(label ? label : id)});
  }
  return types;
}

bool sourceHasAudio(const QString &sourceName)
{
  obs_source_t *source = findSourceByName(sourceName);
  if (!source)
    return false;
  const uint32_t flags = obs_source_get_output_flags(source);
  obs_source_release(source);
  return (flags & OBS_SOURCE_AUDIO) != 0;
}

bool setScene(const QString &sceneName)
{
  obs_source_t *scene = findSourceByName(sceneName);
  if (!scene)
    return false;
  obs_frontend_set_current_scene(scene);
  obs_source_release(scene);
  return true;
}

bool applyItemTransition(const QString &sceneName, const QString &sourceName,
                         bool show, const QString &transitionId,
                         int transitionMs)
{
  if (transitionId.isEmpty())
    return true;

  obs_source_t *sceneSource = findSourceByName(sceneName);
  if (!sceneSource)
    return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) {
    obs_source_release(sceneSource);
    return false;
  }
  obs_sceneitem_t *item =
      obs_scene_find_source(scene, sourceName.toUtf8().constData());
  if (!item) {
    obs_source_release(sceneSource);
    return false;
  }

  // A transicao de mostrar/ocultar e a mesma que o OBS usa no olho da fonte.
  if (transitionId == QLatin1String("none")) {
    obs_sceneitem_set_transition(item, show, nullptr);
  } else {
    obs_source_t *existing = obs_sceneitem_get_transition(item, show);
    const QString existingId =
        existing ? QString::fromUtf8(obs_source_get_id(existing)) : QString();
    if (existingId != transitionId) {
      const QByteArray id = transitionId.toUtf8();
      const char *label = obs_source_get_display_name(id.constData());
      obs_source_t *created = obs_source_create_private(
          id.constData(), label ? label : id.constData(), nullptr);
      if (created) {
        obs_sceneitem_set_transition(item, show, created);
        obs_source_release(created);
      }
    }
  }
  if (transitionMs > 0) {
    obs_sceneitem_set_transition_duration(item, show,
                                          static_cast<uint32_t>(transitionMs));
  }

  obs_source_release(sceneSource);
  return true;
}

bool setSourceVisible(const QString &sceneName, const QString &sourceName,
                      bool visible, const QString &transitionId,
                      int transitionMs)
{
  applyItemTransition(sceneName, sourceName, visible, transitionId,
                      transitionMs);

  obs_source_t *sceneSource = findSourceByName(sceneName);
  if (!sceneSource)
    return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) {
    obs_source_release(sceneSource);
    return false;
  }
  obs_sceneitem_t *item =
      obs_scene_find_source(scene, sourceName.toUtf8().constData());
  if (!item) {
    obs_source_release(sceneSource);
    return false;
  }
  obs_sceneitem_set_visible(item, visible);
  obs_source_release(sceneSource);
  return true;
}

bool setTransition(const QString &transitionName, int durationMs)
{
  if (durationMs >= 0)
    obs_frontend_set_transition_duration(durationMs);

  if (transitionName.isEmpty())
    return true;

  obs_frontend_source_list list = {};
  obs_frontend_get_transitions(&list);
  bool ok = false;
  for (size_t i = 0; i < list.sources.num; ++i) {
    obs_source_t *src = list.sources.array[i];
    if (!src)
      continue;
    if (transitionName == QString::fromUtf8(obs_source_get_name(src))) {
      obs_frontend_set_current_transition(src);
      ok = true;
      break;
    }
  }
  obs_frontend_source_list_free(&list);
  return ok;
}

bool setMute(const QString &sourceName, bool muted)
{
  obs_source_t *source = findSourceByName(sourceName);
  if (!source)
    return false;
  obs_source_set_muted(source, muted);
  obs_source_release(source);
  return true;
}

bool setVolume(const QString &sourceName, double volume)
{
  obs_source_t *source = findSourceByName(sourceName);
  if (!source)
    return false;
  if (volume < 0.0)
    volume = 0.0;
  if (volume > 1.0)
    volume = 1.0;
  obs_source_set_volume(source, static_cast<float>(volume));
  obs_source_release(source);
  return true;
}

bool restartMedia(const QString &sourceName)
{
  obs_source_t *source = findSourceByName(sourceName);
  if (!source)
    return false;
  obs_source_media_restart(source);
  obs_source_release(source);
  return true;
}

bool openUrl(const QString &url)
{
  if (url.isEmpty())
    return false;
  return QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}

bool openRecordingFolder()
{
  char *path = obs_frontend_get_last_recording();
  QString filePath;
  if (path && path[0]) {
    filePath = QString::fromUtf8(path);
    bfree(path);
  } else {
    if (path)
      bfree(path);
    char *dirPath = obs_frontend_get_current_record_output_path();
    if (dirPath && dirPath[0]) {
      const QString folder = QString::fromUtf8(dirPath);
      bfree(dirPath);
#ifdef _WIN32
      const std::wstring w = folder.toStdWString();
      return reinterpret_cast<INT_PTR>(ShellExecuteW(
                 nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) >
             32;
#else
      return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
    }
    if (dirPath)
      bfree(dirPath);
    return false;
  }

  const QFileInfo info(filePath);
  const QString folder = info.absolutePath();
  if (folder.isEmpty() || !QDir(folder).exists())
    return false;

#ifdef _WIN32
  const QString params = QStringLiteral("/select,\"%1\"").arg(
      QDir::toNativeSeparators(filePath));
  const std::wstring explorer = L"explorer.exe";
  const std::wstring wparams = params.toStdWString();
  return reinterpret_cast<INT_PTR>(ShellExecuteW(
             nullptr, L"open", explorer.c_str(), wparams.c_str(), nullptr,
             SW_SHOWNORMAL)) > 32;
#else
  return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

namespace {

constexpr const char *kBlackOverlayName = "Suítes: Tela Preta";

// Descobre qual id de fonte de cor este OBS oferece.
QByteArray colorSourceId()
{
  const char *candidates[] = {"color_source_v3", "color_source_v2",
                              "color_source"};
  const char *id = nullptr;
  for (size_t i = 0; obs_enum_input_types(i, &id); ++i) {
    if (!id)
      continue;
    for (const char *candidate : candidates) {
      if (strcmp(id, candidate) == 0)
        return QByteArray(candidate);
    }
  }
  return QByteArray("color_source_v3");
}

// Garante que exista uma camada preta do tamanho da tela, no topo da cena.
obs_sceneitem_t *ensureBlackOverlay(obs_scene_t *scene)
{
  if (!scene)
    return nullptr;

  obs_sceneitem_t *item = obs_scene_find_source(scene, kBlackOverlayName);
  if (item) {
    obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
    return item;
  }

  obs_video_info ovi = {};
  if (!obs_get_video_info(&ovi)) {
    ovi.base_width = 1920;
    ovi.base_height = 1080;
  }

  obs_source_t *source = obs_get_source_by_name(kBlackOverlayName);
  if (!source) {
    obs_data_t *settings = obs_data_create();
    // 0xFF000000 em ABGR: preto totalmente opaco.
    obs_data_set_int(settings, "color", 0xFF000000);
    obs_data_set_int(settings, "width", ovi.base_width);
    obs_data_set_int(settings, "height", ovi.base_height);
    source = obs_source_create(colorSourceId().constData(), kBlackOverlayName,
                               settings, nullptr);
    obs_data_release(settings);
  }
  if (!source)
    return nullptr;

  item = obs_scene_add(scene, source);
  obs_source_release(source);
  if (!item)
    return nullptr;

  obs_sceneitem_set_visible(item, false);
  obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
  return item;
}

// Prepara a transicao de esmaecer da camada preta, no lado certo.
void setOverlayFade(obs_sceneitem_t *item, bool show, int durationMs)
{
  obs_source_t *existing = obs_sceneitem_get_transition(item, show);
  if (!existing ||
      strcmp(obs_source_get_id(existing), "fade_transition") != 0) {
    obs_source_t *fade = obs_source_create_private(
        "fade_transition", "Suites Screen Fade", nullptr);
    if (fade) {
      obs_sceneitem_set_transition(item, show, fade);
      obs_source_release(fade);
    }
  }
  obs_sceneitem_set_transition_duration(item, show,
                                        static_cast<uint32_t>(durationMs));
}

} // namespace

bool screenFade(const QString &sceneName, bool fadeFromBlack, int durationMs)
{
  if (durationMs <= 0)
    durationMs = 1;

  obs_source_t *sceneSource = sceneName.trimmed().isEmpty()
                                  ? obs_frontend_get_current_scene()
                                  : findSourceByName(sceneName);
  if (!sceneSource)
    return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) {
    obs_source_release(sceneSource);
    return false;
  }

  obs_sceneitem_t *item = ensureBlackOverlay(scene);
  if (!item) {
    obs_source_release(sceneSource);
    return false;
  }

  if (fadeFromBlack) {
    // A tela precisa estar preta antes de clarear. Se a camada nao estiver
    // no ar (primeira gravacao, por exemplo), ela entra na hora, sem fade.
    if (!obs_sceneitem_visible(item)) {
      obs_sceneitem_set_transition(item, true, nullptr);
      obs_sceneitem_set_visible(item, true);
    }
    setOverlayFade(item, false, durationMs);
    obs_sceneitem_set_visible(item, false);
  } else {
    setOverlayFade(item, true, durationMs);
    if (obs_sceneitem_visible(item)) {
      // Ja esta preto: nada a escurecer.
      obs_source_release(sceneSource);
      return true;
    }
    obs_sceneitem_set_visible(item, true);
  }

  obs_source_release(sceneSource);
  return true;
}

bool isRecording()
{
  return obs_frontend_recording_active();
}

bool stopRecording()
{
  if (!obs_frontend_recording_active())
    return false;
  obs_frontend_recording_stop();
  return true;
}

bool isCurrentScene(const QString &sceneName)
{
  obs_source_t *current = obs_frontend_get_current_scene();
  if (!current)
    return false;
  const bool match =
      sceneName == QString::fromUtf8(obs_source_get_name(current));
  obs_source_release(current);
  return match;
}

bool isSourceVisible(const QString &sceneName, const QString &sourceName,
                     bool expectVisible)
{
  obs_source_t *sceneSource = findSourceByName(sceneName);
  if (!sceneSource)
    return false;
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  if (!scene) {
    obs_source_release(sceneSource);
    return false;
  }
  obs_sceneitem_t *item =
      obs_scene_find_source(scene, sourceName.toUtf8().constData());
  const bool visible = item ? obs_sceneitem_visible(item) : false;
  obs_source_release(sceneSource);
  return visible == expectVisible;
}

namespace {

// Sobe o audio do silencio ate o volume escolhido, tirando do mudo antes.
bool fadeInAudio(const QString &sourceName, double targetVolume, int durationMs)
{
  SuiteFader::FadeRequest request;
  request.sourceName = sourceName;
  request.targetVolume = targetVolume;
  request.startVolume = 0.0;
  request.durationMs = durationMs;
  request.unmuteAtStart = true;
  return SuiteFader::startFade(request);
}

// Desce o audio ate o silencio. Quando hideItemAtEnd esta ligado, oculta a
// fonte no fim e devolve o volume original, para o proximo fade in funcionar.
bool fadeOutAudio(const QString &sceneName, const QString &sourceName,
                  int durationMs, bool hideItemAtEnd)
{
  SuiteFader::FadeRequest request;
  request.sceneName = sceneName;
  request.sourceName = sourceName;
  request.targetVolume = 0.0;
  request.durationMs = durationMs;
  request.hideItemWhenDone = hideItemAtEnd;
  request.restoreVolumeWhenDone = hideItemAtEnd;
  return SuiteFader::startFade(request);
}

// Desliza do volume atual ate o volume escolhido, sem mexer no mudo.
bool fadeToVolume(const QString &sourceName, double targetVolume,
                  int durationMs)
{
  SuiteFader::FadeRequest request;
  request.sourceName = sourceName;
  request.targetVolume = targetVolume;
  request.durationMs = durationMs;
  return SuiteFader::startFade(request);
}

} // namespace

bool executeStep(const SuiteStep &step, bool *skipNextOut, int *waitMsOut)
{
  if (skipNextOut)
    *skipNextOut = false;
  if (waitMsOut)
    *waitMsOut = 0;

  const int fadeMs = qMax(0, step.fadeMs);
  const bool waitForFade = step.waitForFade && fadeMs > 0;

  switch (step.type) {
  case SuiteStepType::SetScene:
    if (!step.transition.isEmpty() || step.transitionMs > 0)
      setTransition(step.transition, step.transitionMs);
    return setScene(step.scene);
  case SuiteStepType::SetSourceVisible: {
    const bool withFade = step.fadeAudio && sourceHasAudio(step.source);
    if (!withFade)
      return setSourceVisible(step.scene, step.source, step.visible,
                              step.itemTransitionId, step.itemTransitionMs);

    if (waitForFade && waitMsOut)
      *waitMsOut = fadeMs;

    if (step.visible) {
      // Aparece em silencio e o audio sobe ate o volume escolhido.
      const bool shown =
          setSourceVisible(step.scene, step.source, true,
                           step.itemTransitionId, step.itemTransitionMs);
      const bool faded = fadeInAudio(step.source, step.volume, fadeMs);
      return shown && faded;
    }
    // O audio desce e a fonte so e ocultada quando o fade termina, ja com a
    // transicao de ocultar preparada.
    applyItemTransition(step.scene, step.source, false, step.itemTransitionId,
                        step.itemTransitionMs);
    return fadeOutAudio(step.scene, step.source, fadeMs, true);
  }
  case SuiteStepType::DelayMs:
    return true;
  case SuiteStepType::SetTransition:
    return setTransition(step.transition, step.transitionMs);
  case SuiteStepType::SetMute:
    return setMute(step.source, step.muted);
  case SuiteStepType::SetVolume:
    if (!step.fadeAudio)
      return setVolume(step.source, step.volume);
    if (waitForFade && waitMsOut)
      *waitMsOut = fadeMs;
    return fadeToVolume(step.source, step.volume, fadeMs);
  case SuiteStepType::AudioFade:
    if (waitForFade && waitMsOut)
      *waitMsOut = fadeMs;
    return step.fadeIn ? fadeInAudio(step.source, step.volume, fadeMs)
                       : fadeOutAudio(step.scene, step.source, fadeMs, false);
  case SuiteStepType::ScreenFade: {
    // O fade da tela e sempre esperado: e ele que define o tempo do inicio
    // escuro e do encerramento no preto.
    const int screenMs = qMax(1, step.screenFadeMs);
    if (waitMsOut)
      *waitMsOut = screenMs;
    return screenFade(step.scene, step.fadeIn, screenMs);
  }
  case SuiteStepType::IfCurrentScene:
    if (skipNextOut)
      *skipNextOut = !isCurrentScene(step.scene);
    return true;
  case SuiteStepType::IfSourceVisible:
    if (skipNextOut)
      *skipNextOut =
          !isSourceVisible(step.scene, step.source, step.visible);
    return true;
  case SuiteStepType::IfTrigger:
    // Quem sabe qual foi o gatilho e o motor, entao ele resolve esse passo.
    return true;
  case SuiteStepType::RestartMedia:
    return restartMedia(step.source);
  case SuiteStepType::OpenUrl:
    return openUrl(step.url);
  }
  return false;
}

} // namespace SuiteActions
