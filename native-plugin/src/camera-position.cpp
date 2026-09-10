#include "camera-position.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <graphics/matrix4.h>
#include <graphics/vec3.h>
#include <util/config-file.h>

#include <QChar>
#include <QObject>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr const char *kConfigSection = "OBSOverlayTimeShowCamera";
constexpr const char *kConfigSource = "Source";
constexpr const char *kConfigMargin = "Margin";
constexpr const char *kConfigAllScenes = "AllScenes";

QString g_sourceName;
int g_margin = 24;
bool g_allScenes = false;
bool g_loaded = false;

struct SourceList {
  QStringList names;
};

bool is_video_input(obs_source_t *source)
{
  const uint32_t flags = obs_source_get_output_flags(source);
  if ((flags & OBS_SOURCE_VIDEO) == 0)
    return false;
  return obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT;
}

bool collect_video_source(void *data, obs_source_t *source)
{
  auto *list = static_cast<SourceList *>(data);
  if (!is_video_input(source))
    return true;
  const char *name = obs_source_get_name(source);
  if (name && name[0])
    list->names << QString::fromUtf8(name);
  return true;
}

// "Câmera" nao contem "cam" por causa do acento, entao a comparacao por nome
// ignora os acentos.
QString without_accents(const QString &text)
{
  const QString decomposed = text.normalized(QString::NormalizationForm_D);
  QString plain;
  plain.reserve(decomposed.size());
  for (const QChar character : decomposed) {
    if (character.category() != QChar::Mark_NonSpacing)
      plain.append(character);
  }
  return plain;
}

struct GuessResult {
  QString byType;
  QString byName;
};

// A camera e escolhida primeiro pelo tipo da fonte (dispositivo de captura de
// video) e so depois pelo nome, que o usuario pode ter mudado.
bool guess_camera_source(void *data, obs_source_t *source)
{
  auto *guess = static_cast<GuessResult *>(data);
  if (!is_video_input(source))
    return true;

  const char *rawName = obs_source_get_name(source);
  if (!rawName || !rawName[0])
    return true;
  const QString name = QString::fromUtf8(rawName);

  const char *id = obs_source_get_id(source);
  if (id && guess->byType.isEmpty() &&
      (strcmp(id, "dshow_input") == 0 || strcmp(id, "av_capture_input") == 0 ||
       strcmp(id, "v4l2_input") == 0)) {
    guess->byType = name;
  }

  if (guess->byName.isEmpty() &&
      without_accents(name).contains(QStringLiteral("cam"),
                                     Qt::CaseInsensitive)) {
    guess->byName = name;
  }
  return true;
}

// Posicao horizontal e vertical de cada canto, separadas para o calculo.
enum class Side { Start, Center, End };

Side horizontal_side(CameraPosition::Anchor anchor)
{
  switch (anchor) {
  case CameraPosition::Anchor::TopLeft:
  case CameraPosition::Anchor::BottomLeft:
    return Side::Start;
  case CameraPosition::Anchor::TopCenter:
  case CameraPosition::Anchor::BottomCenter:
    return Side::Center;
  case CameraPosition::Anchor::TopRight:
  case CameraPosition::Anchor::BottomRight:
    return Side::End;
  }
  return Side::Start;
}

Side vertical_side(CameraPosition::Anchor anchor)
{
  switch (anchor) {
  case CameraPosition::Anchor::TopLeft:
  case CameraPosition::Anchor::TopCenter:
  case CameraPosition::Anchor::TopRight:
    return Side::Start;
  case CameraPosition::Anchor::BottomLeft:
  case CameraPosition::Anchor::BottomCenter:
  case CameraPosition::Anchor::BottomRight:
    return Side::End;
  }
  return Side::Start;
}

// Retangulo que a fonte realmente ocupa na tela. Vem da matriz final do item,
// entao ja considera escala, espelhamento, caixa delimitadora, recorte e
// rotacao -- coisas que o alinhamento sozinho nao resolve.
void item_screen_rect(obs_sceneitem_t *item, float *left, float *top,
                      float *right, float *bottom)
{
  struct matrix4 box;
  obs_sceneitem_get_box_transform(item, &box);

  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  for (int corner = 0; corner < 4; ++corner) {
    struct vec3 point;
    vec3_set(&point, corner == 1 || corner == 3 ? 1.0f : 0.0f,
             corner >= 2 ? 1.0f : 0.0f, 0.0f);
    struct vec3 screen;
    vec3_transform(&screen, &point, &box);
    if (corner == 0) {
      minX = maxX = screen.x;
      minY = maxY = screen.y;
      continue;
    }
    minX = std::min(minX, screen.x);
    maxX = std::max(maxX, screen.x);
    minY = std::min(minY, screen.y);
    maxY = std::max(maxY, screen.y);
  }

  *left = minX;
  *top = minY;
  *right = maxX;
  *bottom = maxY;
}

// Onde a borda inicial do retangulo precisa ficar. Fonte maior que a tela e
// centralizada, para nunca sobrar imagem fora do quadro.
float target_start(Side side, float size, float canvas, float margin)
{
  if (size + margin * 2.0f > canvas)
    return (canvas - size) * 0.5f;
  switch (side) {
  case Side::Start:
    return margin;
  case Side::Center:
    return (canvas - size) * 0.5f;
  case Side::End:
    return canvas - margin - size;
  }
  return margin;
}

// Rota no sentido do relogio, para o atalho de girar fazer a volta na tela.
const CameraPosition::Anchor kRoute[] = {
    CameraPosition::Anchor::TopLeft,     CameraPosition::Anchor::TopCenter,
    CameraPosition::Anchor::TopRight,    CameraPosition::Anchor::BottomRight,
    CameraPosition::Anchor::BottomCenter, CameraPosition::Anchor::BottomLeft,
};
constexpr int kRouteCount = 6;

// Distancia de encaixe usada para descobrir em qual canto a fonte esta.
constexpr float kSnapTolerance = 24.0f;

bool side_from_position(float start, float end, float canvas, float margin,
                        Side *side)
{
  const float size = end - start;
  const float distanceStart = std::fabs(start - margin);
  const float distanceEnd = std::fabs(end - (canvas - margin));
  const float distanceCenter = std::fabs((start + end) * 0.5f - canvas * 0.5f);

  const float best = std::min({distanceStart, distanceCenter, distanceEnd});
  if (best > kSnapTolerance || size <= 0.0f)
    return false;

  if (best == distanceStart)
    *side = Side::Start;
  else if (best == distanceEnd)
    *side = Side::End;
  else
    *side = Side::Center;
  return true;
}

bool anchor_from_sides(Side horizontal, Side vertical,
                       CameraPosition::Anchor *anchor)
{
  for (const CameraPosition::Anchor candidate : kRoute) {
    if (horizontal_side(candidate) != horizontal ||
        vertical_side(candidate) != vertical)
      continue;
    *anchor = candidate;
    return true;
  }
  return false;
}

bool move_in_scene(obs_scene_t *scene, const QString &name,
                   CameraPosition::Anchor anchor)
{
  if (!scene)
    return false;
  obs_sceneitem_t *item =
      obs_scene_find_source_recursive(scene, name.toUtf8().constData());
  if (!item)
    return false;

  struct obs_video_info ovi;
  if (!obs_get_video_info(&ovi))
    return false;

  float left = 0.0f;
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  item_screen_rect(item, &left, &top, &right, &bottom);

  const float margin = static_cast<float>(CameraPosition::margin());
  const float canvasWidth = static_cast<float>(ovi.base_width);
  const float canvasHeight = static_cast<float>(ovi.base_height);
  const float targetLeft = target_start(horizontal_side(anchor), right - left,
                                        canvasWidth, margin);
  const float targetTop = target_start(vertical_side(anchor), bottom - top,
                                       canvasHeight, margin);

  // Move pelo deslocamento medido, entao o alinhamento, a escala e a caixa
  // delimitadora que o usuario escolheu continuam intactos.
  struct obs_transform_info info;
  obs_sceneitem_get_info2(item, &info);
  info.pos.x += targetLeft - left;
  info.pos.y += targetTop - top;
  obs_sceneitem_set_info2(item, &info);
  return true;
}

struct MoveAllContext {
  QString name;
  CameraPosition::Anchor anchor;
  bool moved = false;
};

bool move_in_every_scene(void *data, obs_source_t *source)
{
  auto *ctx = static_cast<MoveAllContext *>(data);
  obs_scene_t *scene = obs_scene_from_source(source);
  if (scene && move_in_scene(scene, ctx->name, ctx->anchor))
    ctx->moved = true;
  return true;
}

} // namespace

namespace CameraPosition {

QString anchorConfigName(Anchor anchor)
{
  switch (anchor) {
  case Anchor::TopLeft:
    return QStringLiteral("camera_top_left");
  case Anchor::TopCenter:
    return QStringLiteral("camera_top_center");
  case Anchor::TopRight:
    return QStringLiteral("camera_top_right");
  case Anchor::BottomLeft:
    return QStringLiteral("camera_bottom_left");
  case Anchor::BottomCenter:
    return QStringLiteral("camera_bottom_center");
  case Anchor::BottomRight:
    return QStringLiteral("camera_bottom_right");
  }
  return QStringLiteral("camera_top_left");
}

QString anchorLabel(Anchor anchor)
{
  switch (anchor) {
  case Anchor::TopLeft:
    return QObject::tr("Superior esquerdo");
  case Anchor::TopCenter:
    return QObject::tr("Superior centro");
  case Anchor::TopRight:
    return QObject::tr("Superior direito");
  case Anchor::BottomLeft:
    return QObject::tr("Inferior esquerdo");
  case Anchor::BottomCenter:
    return QObject::tr("Inferior centro");
  case Anchor::BottomRight:
    return QObject::tr("Inferior direito");
  }
  return QString();
}

QString sourceName()
{
  load();
  // Nada escolhido ainda: adota a camera automaticamente, para os atalhos
  // funcionarem sem precisar configurar nada antes.
  if (g_sourceName.isEmpty()) {
    const QString guess = guessSourceName();
    if (!guess.isEmpty()) {
      blog(LOG_INFO,
           "[obs-overlay-time-show] atalhos de posicao vao mover a fonte "
           "\"%s\"",
           guess.toUtf8().constData());
      setSourceName(guess);
    }
  }
  return g_sourceName;
}

QString guessSourceName()
{
  GuessResult guess;
  obs_enum_sources(guess_camera_source, &guess);
  return guess.byType.isEmpty() ? guess.byName : guess.byType;
}

void setSourceName(const QString &name)
{
  g_sourceName = name;
  save();
}

int margin()
{
  load();
  return g_margin;
}

void setMargin(int pixels)
{
  g_margin = pixels < 0 ? 0 : pixels;
  save();
}

bool allScenes()
{
  load();
  return g_allScenes;
}

void setAllScenes(bool enabled)
{
  g_allScenes = enabled;
  save();
}

QStringList videoSources()
{
  SourceList list;
  obs_enum_sources(collect_video_source, &list);
  list.names.sort(Qt::CaseInsensitive);
  return list.names;
}

bool apply(Anchor anchor)
{
  const QString name = sourceName();
  if (name.isEmpty()) {
    blog(LOG_INFO,
         "[obs-overlay-time-show] nenhuma fonte escolhida para os atalhos de "
         "posicao da camera");
    return false;
  }

  bool moved = false;
  if (allScenes()) {
    MoveAllContext ctx;
    ctx.name = name;
    ctx.anchor = anchor;
    obs_enum_scenes(move_in_every_scene, &ctx);
    moved = ctx.moved;
  } else {
    obs_source_t *current = obs_frontend_get_current_scene();
    if (current) {
      moved = move_in_scene(obs_scene_from_source(current), name, anchor);
      obs_source_release(current);
    }
  }

  if (!moved) {
    blog(LOG_INFO,
         "[obs-overlay-time-show] fonte \"%s\" nao esta na cena, posicao nao "
         "mudou",
         name.toUtf8().constData());
  }
  return moved;
}

bool currentAnchor(Anchor *anchor)
{
  const QString name = sourceName();
  if (name.isEmpty() || !anchor)
    return false;

  obs_source_t *current = obs_frontend_get_current_scene();
  if (!current)
    return false;

  bool found = false;
  obs_scene_t *scene = obs_scene_from_source(current);
  struct obs_video_info ovi;
  if (scene && obs_get_video_info(&ovi)) {
    obs_sceneitem_t *item =
        obs_scene_find_source_recursive(scene, name.toUtf8().constData());
    if (item) {
      float left = 0.0f;
      float top = 0.0f;
      float right = 0.0f;
      float bottom = 0.0f;
      item_screen_rect(item, &left, &top, &right, &bottom);

      const float edge = static_cast<float>(margin());
      Side horizontal = Side::Start;
      Side vertical = Side::Start;
      found = side_from_position(left, right,
                                 static_cast<float>(ovi.base_width), edge,
                                 &horizontal) &&
              side_from_position(top, bottom,
                                 static_cast<float>(ovi.base_height), edge,
                                 &vertical) &&
              anchor_from_sides(horizontal, vertical, anchor);
    }
  }
  obs_source_release(current);
  return found;
}

bool cycle(int direction)
{
  const int step = direction < 0 ? -1 : 1;

  // Sem posicao conhecida, o primeiro toque leva para o inicio da rota.
  Anchor anchor = Anchor::TopLeft;
  int index = step > 0 ? 0 : kRouteCount - 1;
  if (currentAnchor(&anchor)) {
    for (int i = 0; i < kRouteCount; ++i) {
      if (kRoute[i] != anchor)
        continue;
      index = (i + step + kRouteCount) % kRouteCount;
      break;
    }
  }

  return apply(kRoute[index]);
}

void load()
{
  if (g_loaded)
    return;
  reload();
}

void reload()
{
  config_t *config = obs_frontend_get_profile_config();
  if (!config)
    return;

  g_sourceName.clear();
  g_margin = 24;
  g_allScenes = false;

  const char *name = config_get_string(config, kConfigSection, kConfigSource);
  if (name)
    g_sourceName = QString::fromUtf8(name);
  if (config_has_user_value(config, kConfigSection, kConfigMargin))
    g_margin = static_cast<int>(
        config_get_int(config, kConfigSection, kConfigMargin));
  if (config_has_user_value(config, kConfigSection, kConfigAllScenes))
    g_allScenes = config_get_bool(config, kConfigSection, kConfigAllScenes);
  g_loaded = true;
}

void save()
{
  config_t *config = obs_frontend_get_profile_config();
  if (!config)
    return;
  config_set_string(config, kConfigSection, kConfigSource,
                    g_sourceName.toUtf8().constData());
  config_set_int(config, kConfigSection, kConfigMargin, g_margin);
  config_set_bool(config, kConfigSection, kConfigAllScenes, g_allScenes);
  config_save(config);
}

} // namespace CameraPosition
