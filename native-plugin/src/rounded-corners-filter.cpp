#include "rounded-corners-filter.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr const char *kFilterId = "obs_overlay_time_show_rounded_corners";

constexpr const char *kSettingShape = "shape";
constexpr const char *kSettingRadius = "radius";
constexpr const char *kSettingSoftness = "softness";
constexpr const char *kSettingTopLeft = "corner_top_left";
constexpr const char *kSettingTopRight = "corner_top_right";
constexpr const char *kSettingBottomRight = "corner_bottom_right";
constexpr const char *kSettingBottomLeft = "corner_bottom_left";
constexpr const char *kSettingCropLeft = "crop_left";
constexpr const char *kSettingCropRight = "crop_right";
constexpr const char *kSettingCropTop = "crop_top";
constexpr const char *kSettingCropBottom = "crop_bottom";
constexpr const char *kSettingAutoCrop = "auto_crop";
constexpr const char *kSettingBlackLevel = "black_level";
constexpr const char *kShapeRounded = "rounded";
constexpr const char *kShapeCircle = "circle";

// O shader fica embutido para o filtro nao depender de arquivo instalado.
// A entrada do OBS vem com alfa pre-multiplicado, por isso a cobertura
// multiplica o RGBA inteiro em vez de mexer so no alfa.
const char *kEffectSource = R"EFFECT(
uniform float4x4 ViewProj;
uniform texture2d image;

uniform float2 pixel_size;
uniform float4 corner_radius;
uniform float softness;
uniform float circle_mode;
uniform float2 uv_scale;
uniform float2 uv_offset;

sampler_state textureSampler {
	Filter    = Linear;
	AddressU  = Clamp;
	AddressV  = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

float shape_distance(float2 uv)
{
	float2 half_size = pixel_size * 0.5;
	float2 p = uv * pixel_size - half_size;
	float shortest = min(half_size.x, half_size.y);

	if (circle_mode > 0.5)
		return length(p) - shortest;

	float radius = (p.x < 0.0)
		? ((p.y < 0.0) ? corner_radius.x : corner_radius.w)
		: ((p.y < 0.0) ? corner_radius.y : corner_radius.z);
	radius = clamp(radius, 0.0, shortest);

	float2 q = abs(p) - half_size + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float4 PSRoundedCorners(VertData v_in) : TARGET
{
	// v_in.uv percorre a area recortada; uv_scale/uv_offset levam essa
	// coordenada de volta para o quadro original da fonte.
	float2 uv = v_in.uv * uv_scale + uv_offset;
	float4 rgba = image.Sample(textureSampler, uv);
	float dist = shape_distance(v_in.uv);
	float edge = max(softness, 0.001);
	float coverage = saturate(0.5 - dist / edge);
	return rgba * coverage;
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader  = PSRoundedCorners(v_in);
	}
}
)EFFECT";

struct FilterData {
  obs_source_t *context = nullptr;
  gs_effect_t *effect = nullptr;
  gs_eparam_t *paramPixelSize = nullptr;
  gs_eparam_t *paramCornerRadius = nullptr;
  gs_eparam_t *paramSoftness = nullptr;
  gs_eparam_t *paramCircleMode = nullptr;
  gs_eparam_t *paramUvScale = nullptr;
  gs_eparam_t *paramUvOffset = nullptr;

  bool circle = false;
  float radius = 64.0f;
  float softness = 1.5f;
  bool topLeft = true;
  bool topRight = true;
  bool bottomRight = true;
  bool bottomLeft = true;

  int cropLeft = 0;
  int cropRight = 0;
  int cropTop = 0;
  int cropBottom = 0;

  // Deteccao das tarjas preta da propria imagem.
  bool autoCrop = false;
  int blackLevel = 24;
  int autoLeft = 0;
  int autoRight = 0;
  int autoTop = 0;
  int autoBottom = 0;
  bool needsDetect = false;
  float sinceDetect = 0.0f;
  gs_texrender_t *sampleRender = nullptr;
  gs_stagesurf_t *sampleSurface = nullptr;
  uint32_t sampleWidth = 0;
  uint32_t sampleHeight = 0;

  // Recalculados a cada quadro, quando o tamanho da fonte e conhecido.
  uint32_t width = 0;
  uint32_t height = 0;
  struct vec2 uvScale = {1.0f, 1.0f};
  struct vec2 uvOffset = {0.0f, 0.0f};
};

const char *filter_get_name(void *)
{
  return obs_module_text("OBSOverlayTimeShow.Filter.RoundedCorners");
}

void filter_update(void *data, obs_data_t *settings)
{
  auto *filter = static_cast<FilterData *>(data);
  const char *shape = obs_data_get_string(settings, kSettingShape);
  filter->circle = shape && strcmp(shape, kShapeCircle) == 0;
  filter->radius = static_cast<float>(obs_data_get_int(settings, kSettingRadius));
  filter->softness =
      static_cast<float>(obs_data_get_double(settings, kSettingSoftness));
  filter->topLeft = obs_data_get_bool(settings, kSettingTopLeft);
  filter->topRight = obs_data_get_bool(settings, kSettingTopRight);
  filter->bottomRight = obs_data_get_bool(settings, kSettingBottomRight);
  filter->bottomLeft = obs_data_get_bool(settings, kSettingBottomLeft);
  filter->cropLeft =
      static_cast<int>(obs_data_get_int(settings, kSettingCropLeft));
  filter->cropRight =
      static_cast<int>(obs_data_get_int(settings, kSettingCropRight));
  filter->cropTop =
      static_cast<int>(obs_data_get_int(settings, kSettingCropTop));
  filter->cropBottom =
      static_cast<int>(obs_data_get_int(settings, kSettingCropBottom));

  const bool autoCrop = obs_data_get_bool(settings, kSettingAutoCrop);
  filter->blackLevel =
      static_cast<int>(obs_data_get_int(settings, kSettingBlackLevel));
  if (autoCrop) {
    // Ligar (ou mexer no nivel de preto) refaz a medida no proximo quadro.
    filter->needsDetect = true;
    filter->sinceDetect = 0.0f;
  } else if (filter->autoCrop) {
    filter->autoLeft = 0;
    filter->autoRight = 0;
    filter->autoTop = 0;
    filter->autoBottom = 0;
  }
  filter->autoCrop = autoCrop;
}

// Renderiza uma amostra pequena da fonte, le de volta na CPU e acha a caixa
// com imagem de verdade. Amostra reduzida porque para achar tarja preta nao
// precisa de resolucao cheia, e a leitura da GPU e caro.
void detect_black_borders(FilterData *filter, obs_source_t *target,
                         uint32_t baseWidth, uint32_t baseHeight)
{
  filter->needsDetect = false;

  constexpr uint32_t kMaxSide = 256;
  uint32_t sampleW = baseWidth;
  uint32_t sampleH = baseHeight;
  if (baseWidth >= baseHeight && baseWidth > kMaxSide) {
    sampleW = kMaxSide;
    sampleH = std::max(1u, baseHeight * kMaxSide / baseWidth);
  } else if (baseHeight > kMaxSide) {
    sampleH = kMaxSide;
    sampleW = std::max(1u, baseWidth * kMaxSide / baseHeight);
  }

  if (!filter->sampleRender)
    filter->sampleRender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  if (!filter->sampleSurface || filter->sampleWidth != sampleW ||
      filter->sampleHeight != sampleH) {
    gs_stagesurface_destroy(filter->sampleSurface);
    filter->sampleSurface = gs_stagesurface_create(sampleW, sampleH, GS_RGBA);
    filter->sampleWidth = sampleW;
    filter->sampleHeight = sampleH;
  }
  if (!filter->sampleRender || !filter->sampleSurface)
    return;

  gs_texrender_reset(filter->sampleRender);
  if (!gs_texrender_begin(filter->sampleRender, sampleW, sampleH))
    return;

  struct vec4 clear;
  vec4_zero(&clear);
  gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
  gs_ortho(0.0f, static_cast<float>(baseWidth), 0.0f,
           static_cast<float>(baseHeight), -100.0f, 100.0f);
  gs_blend_state_push();
  gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
  obs_source_video_render(target);
  gs_blend_state_pop();
  gs_texrender_end(filter->sampleRender);

  gs_stage_texture(filter->sampleSurface,
                   gs_texrender_get_texture(filter->sampleRender));

  uint8_t *pixels = nullptr;
  uint32_t linesize = 0;
  if (!gs_stagesurface_map(filter->sampleSurface, &pixels, &linesize))
    return;

  std::vector<int> columnHits(sampleW, 0);
  std::vector<int> rowHits(sampleH, 0);
  const int level = std::clamp(filter->blackLevel, 0, 200);

  for (uint32_t y = 0; y < sampleH; ++y) {
    const uint8_t *row = pixels + static_cast<size_t>(y) * linesize;
    for (uint32_t x = 0; x < sampleW; ++x) {
      const uint8_t *px = row + static_cast<size_t>(x) * 4;
      const int brightest = std::max({px[0], px[1], px[2]});
      if (px[3] > 16 && brightest > level) {
        ++columnHits[x];
        ++rowHits[y];
      }
    }
  }
  gs_stagesurface_unmap(filter->sampleSurface);

  // Uma coluna/linha so conta como imagem se boa parte dela nao for preta,
  // para um pixel perdido nao atrapalhar a medida.
  const int minColumn = std::max(2, static_cast<int>(sampleH) / 50);
  const int minRow = std::max(2, static_cast<int>(sampleW) / 50);

  auto bounds = [](const std::vector<int> &hits, int minimum,
                   int *first, int *last) {
    *first = -1;
    *last = -1;
    for (size_t i = 0; i < hits.size(); ++i) {
      if (hits[i] < minimum)
        continue;
      if (*first < 0)
        *first = static_cast<int>(i);
      *last = static_cast<int>(i);
    }
  };

  int firstCol = -1, lastCol = -1, firstRow = -1, lastRow = -1;
  bounds(columnHits, minColumn, &firstCol, &lastCol);
  bounds(rowHits, minRow, &firstRow, &lastRow);
  if (firstCol < 0 || firstRow < 0)
    return; // Quadro todo escuro: mantem a medida anterior.

  const int litW = lastCol - firstCol + 1;
  const int litH = lastRow - firstRow + 1;
  if (litW * 10 < static_cast<int>(sampleW) ||
      litH * 10 < static_cast<int>(sampleH)) {
    return; // Sobrou muito pouco: provavelmente a cena so esta escura.
  }

  const float scaleX = static_cast<float>(baseWidth) /
                       static_cast<float>(sampleW);
  const float scaleY = static_cast<float>(baseHeight) /
                       static_cast<float>(sampleH);
  // Folga de 2 pixels para nao sobrar a linha de transicao da tarja.
  constexpr int kInset = 2;
  const int left = static_cast<int>(std::floor(firstCol * scaleX));
  const int right = static_cast<int>(baseWidth) -
                    static_cast<int>(std::ceil((lastCol + 1) * scaleX));
  const int top = static_cast<int>(std::floor(firstRow * scaleY));
  const int bottom = static_cast<int>(baseHeight) -
                     static_cast<int>(std::ceil((lastRow + 1) * scaleY));

  const int maxX = static_cast<int>(baseWidth) / 2 - 4;
  const int maxY = static_cast<int>(baseHeight) / 2 - 4;
  filter->autoLeft = std::clamp(left > 0 ? left + kInset : 0, 0, maxX);
  filter->autoRight = std::clamp(right > 0 ? right + kInset : 0, 0, maxX);
  filter->autoTop = std::clamp(top > 0 ? top + kInset : 0, 0, maxY);
  filter->autoBottom = std::clamp(bottom > 0 ? bottom + kInset : 0, 0, maxY);
}

// O tamanho da fonte so e confiavel no tick, por isso o recorte e o mapeamento
// de coordenadas sao recalculados aqui e reaproveitados na renderizacao.
void filter_tick(void *data, float seconds)
{
  auto *filter = static_cast<FilterData *>(data);
  obs_source_t *target = obs_filter_get_target(filter->context);
  const uint32_t baseWidth = target ? obs_source_get_base_width(target) : 0;
  const uint32_t baseHeight = target ? obs_source_get_base_height(target) : 0;
  if (baseWidth == 0 || baseHeight == 0) {
    filter->width = 0;
    filter->height = 0;
    return;
  }

  // Remede de vez em quando: se a camera girar ou trocar de modo, a tarja
  // muda de lugar e o recorte se ajusta sozinho.
  if (filter->autoCrop) {
    filter->sinceDetect += seconds;
    if (filter->sinceDetect >= 2.0f) {
      filter->sinceDetect = 0.0f;
      filter->needsDetect = true;
    }
  }

  const int cropped_w =
      std::max(1, static_cast<int>(baseWidth) - filter->autoLeft -
                      filter->cropLeft - filter->autoRight -
                      filter->cropRight);
  const int cropped_h =
      std::max(1, static_cast<int>(baseHeight) - filter->autoTop -
                      filter->cropTop - filter->autoBottom -
                      filter->cropBottom);
  filter->width = static_cast<uint32_t>(cropped_w);
  filter->height = static_cast<uint32_t>(cropped_h);

  vec2_set(&filter->uvScale,
           static_cast<float>(cropped_w) / static_cast<float>(baseWidth),
           static_cast<float>(cropped_h) / static_cast<float>(baseHeight));
  vec2_set(&filter->uvOffset,
           static_cast<float>(filter->autoLeft + filter->cropLeft) /
               static_cast<float>(baseWidth),
           static_cast<float>(filter->autoTop + filter->cropTop) /
               static_cast<float>(baseHeight));
}

uint32_t filter_width(void *data)
{
  return static_cast<FilterData *>(data)->width;
}

uint32_t filter_height(void *data)
{
  return static_cast<FilterData *>(data)->height;
}

void *filter_create(obs_data_t *settings, obs_source_t *context)
{
  auto *filter = new FilterData();
  filter->context = context;

  obs_enter_graphics();
  char *error = nullptr;
  filter->effect = gs_effect_create(kEffectSource, kFilterId, &error);
  if (!filter->effect) {
    blog(LOG_ERROR,
         "[obs-overlay-time-show] falha ao compilar o shader de cantos "
         "arredondados: %s",
         error ? error : "erro desconhecido");
  } else {
    filter->paramPixelSize =
        gs_effect_get_param_by_name(filter->effect, "pixel_size");
    filter->paramCornerRadius =
        gs_effect_get_param_by_name(filter->effect, "corner_radius");
    filter->paramSoftness =
        gs_effect_get_param_by_name(filter->effect, "softness");
    filter->paramCircleMode =
        gs_effect_get_param_by_name(filter->effect, "circle_mode");
    filter->paramUvScale =
        gs_effect_get_param_by_name(filter->effect, "uv_scale");
    filter->paramUvOffset =
        gs_effect_get_param_by_name(filter->effect, "uv_offset");
  }
  bfree(error);
  obs_leave_graphics();

  if (!filter->effect) {
    delete filter;
    return nullptr;
  }

  filter_update(filter, settings);
  return filter;
}

void filter_destroy(void *data)
{
  auto *filter = static_cast<FilterData *>(data);
  if (!filter)
    return;
  obs_enter_graphics();
  if (filter->effect)
    gs_effect_destroy(filter->effect);
  if (filter->sampleSurface)
    gs_stagesurface_destroy(filter->sampleSurface);
  if (filter->sampleRender)
    gs_texrender_destroy(filter->sampleRender);
  obs_leave_graphics();
  delete filter;
}

void filter_defaults(obs_data_t *settings)
{
  obs_data_set_default_string(settings, kSettingShape, kShapeRounded);
  obs_data_set_default_int(settings, kSettingRadius, 64);
  obs_data_set_default_double(settings, kSettingSoftness, 1.5);
  obs_data_set_default_bool(settings, kSettingTopLeft, true);
  obs_data_set_default_bool(settings, kSettingTopRight, true);
  obs_data_set_default_bool(settings, kSettingBottomRight, true);
  obs_data_set_default_bool(settings, kSettingBottomLeft, true);
  obs_data_set_default_int(settings, kSettingCropLeft, 0);
  obs_data_set_default_int(settings, kSettingCropRight, 0);
  obs_data_set_default_int(settings, kSettingCropTop, 0);
  obs_data_set_default_int(settings, kSettingCropBottom, 0);
  obs_data_set_default_bool(settings, kSettingAutoCrop, true);
  obs_data_set_default_int(settings, kSettingBlackLevel, 24);
}

bool auto_crop_modified(obs_properties_t *props, obs_property_t *,
                        obs_data_t *settings)
{
  const bool automatic = obs_data_get_bool(settings, kSettingAutoCrop);
  obs_property_set_visible(obs_properties_get(props, kSettingBlackLevel),
                           automatic);
  return true;
}

// No circulo perfeito o raio e os cantos individuais nao fazem sentido.
bool shape_modified(obs_properties_t *props, obs_property_t *,
                    obs_data_t *settings)
{
  const char *shape = obs_data_get_string(settings, kSettingShape);
  const bool circle = shape && strcmp(shape, kShapeCircle) == 0;
  obs_property_set_visible(obs_properties_get(props, kSettingRadius), !circle);
  obs_property_set_visible(obs_properties_get(props, "corners"), !circle);
  return true;
}

obs_properties_t *filter_properties(void *)
{
  obs_properties_t *props = obs_properties_create();

  obs_property_t *shape = obs_properties_add_list(
      props, kSettingShape, obs_module_text("OBSOverlayTimeShow.Filter.Shape"),
      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
  obs_property_list_add_string(
      shape, obs_module_text("OBSOverlayTimeShow.Filter.Shape.Rounded"),
      kShapeRounded);
  obs_property_list_add_string(
      shape, obs_module_text("OBSOverlayTimeShow.Filter.Shape.Circle"),
      kShapeCircle);
  obs_property_set_long_description(
      shape, obs_module_text("OBSOverlayTimeShow.Filter.Shape.Hint"));
  obs_property_set_modified_callback(shape, shape_modified);

  obs_properties_add_int_slider(
      props, kSettingRadius,
      obs_module_text("OBSOverlayTimeShow.Filter.Radius"), 0, 1000, 1);

  obs_properties_add_float_slider(
      props, kSettingSoftness,
      obs_module_text("OBSOverlayTimeShow.Filter.Softness"), 0.0, 20.0, 0.1);

  obs_properties_t *corners = obs_properties_create();
  obs_properties_add_bool(
      corners, kSettingTopLeft,
      obs_module_text("OBSOverlayTimeShow.Filter.Corner.TopLeft"));
  obs_properties_add_bool(
      corners, kSettingTopRight,
      obs_module_text("OBSOverlayTimeShow.Filter.Corner.TopRight"));
  obs_properties_add_bool(
      corners, kSettingBottomLeft,
      obs_module_text("OBSOverlayTimeShow.Filter.Corner.BottomLeft"));
  obs_properties_add_bool(
      corners, kSettingBottomRight,
      obs_module_text("OBSOverlayTimeShow.Filter.Corner.BottomRight"));
  obs_properties_add_group(
      props, "corners", obs_module_text("OBSOverlayTimeShow.Filter.Corners"),
      OBS_GROUP_NORMAL, corners);

  // Camera com tarja preta: recortar antes faz a forma usar so a area util.
  obs_properties_t *crop = obs_properties_create();
  obs_property_t *hint = obs_properties_add_text(
      crop, "crop_hint", obs_module_text("OBSOverlayTimeShow.Filter.Crop.Hint"),
      OBS_TEXT_INFO);
  obs_property_set_enabled(hint, false);
  obs_property_t *automatic = obs_properties_add_bool(
      crop, kSettingAutoCrop,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.Auto"));
  obs_property_set_long_description(
      automatic, obs_module_text("OBSOverlayTimeShow.Filter.Crop.Auto.Hint"));
  obs_property_set_modified_callback(automatic, auto_crop_modified);

  obs_properties_add_int_slider(
      crop, kSettingBlackLevel,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.BlackLevel"), 0, 120, 1);

  obs_properties_add_int_slider(
      crop, kSettingCropLeft,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.Left"), 0, 4096, 1);
  obs_properties_add_int_slider(
      crop, kSettingCropRight,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.Right"), 0, 4096, 1);
  obs_properties_add_int_slider(
      crop, kSettingCropTop,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.Top"), 0, 4096, 1);
  obs_properties_add_int_slider(
      crop, kSettingCropBottom,
      obs_module_text("OBSOverlayTimeShow.Filter.Crop.Bottom"), 0, 4096, 1);
  obs_properties_add_group(
      props, "crop", obs_module_text("OBSOverlayTimeShow.Filter.Crop"),
      OBS_GROUP_NORMAL, crop);

  return props;
}

void filter_render(void *data, gs_effect_t *)
{
  auto *filter = static_cast<FilterData *>(data);
  if (!filter || !filter->effect) {
    if (filter)
      obs_source_skip_video_filter(filter->context);
    return;
  }

  obs_source_t *target = obs_filter_get_target(filter->context);
  if (filter->needsDetect && target) {
    const uint32_t baseWidth = obs_source_get_base_width(target);
    const uint32_t baseHeight = obs_source_get_base_height(target);
    if (baseWidth && baseHeight)
      detect_black_borders(filter, target, baseWidth, baseHeight);
  }

  const uint32_t width = filter->width;
  const uint32_t height = filter->height;
  if (width == 0 || height == 0) {
    obs_source_skip_video_filter(filter->context);
    return;
  }

  // O recorte muda as coordenadas de textura, entao a fonte nao pode se
  // desenhar direto no destino.
  if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
                                       OBS_NO_DIRECT_RENDERING))
    return;

  struct vec2 pixelSize;
  vec2_set(&pixelSize, static_cast<float>(width), static_cast<float>(height));
  gs_effect_set_vec2(filter->paramPixelSize, &pixelSize);
  gs_effect_set_vec2(filter->paramUvScale, &filter->uvScale);
  gs_effect_set_vec2(filter->paramUvOffset, &filter->uvOffset);

  // Canto desmarcado fica reto: raio zero.
  struct vec4 radii;
  vec4_set(&radii, filter->topLeft ? filter->radius : 0.0f,
           filter->topRight ? filter->radius : 0.0f,
           filter->bottomRight ? filter->radius : 0.0f,
           filter->bottomLeft ? filter->radius : 0.0f);
  gs_effect_set_vec4(filter->paramCornerRadius, &radii);

  gs_effect_set_float(filter->paramSoftness, filter->softness);
  gs_effect_set_float(filter->paramCircleMode, filter->circle ? 1.0f : 0.0f);

  // A saida ja sai com alfa pre-multiplicado.
  gs_blend_state_push();
  gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
  obs_source_process_filter_end(filter->context, filter->effect, width, height);
  gs_blend_state_pop();
}

} // namespace

namespace RoundedCornersFilter {

void registerFilter()
{
  struct obs_source_info info = {};
  info.id = kFilterId;
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_VIDEO;
  info.get_name = filter_get_name;
  info.create = filter_create;
  info.destroy = filter_destroy;
  info.update = filter_update;
  info.get_defaults = filter_defaults;
  info.get_properties = filter_properties;
  info.video_tick = filter_tick;
  info.video_render = filter_render;
  info.get_width = filter_width;
  info.get_height = filter_height;

  obs_register_source(&info);
}

} // namespace RoundedCornersFilter
