#include "recording-timer-overlay.hpp"

#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <functional>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-overlay-time-show", "en-US")

namespace {
RecordingTimerOverlay *overlay = nullptr;
constexpr int kMoveStep = 24;

obs_hotkey_id hotkey_up = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id hotkey_down = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id hotkey_left = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id hotkey_right = OBS_INVALID_HOTKEY_ID;

constexpr const char *kHotkeyNameUp = "obs-overlay-time-show.move_up";
constexpr const char *kHotkeyNameDown = "obs-overlay-time-show.move_down";
constexpr const char *kHotkeyNameLeft = "obs-overlay-time-show.move_left";
constexpr const char *kHotkeyNameRight = "obs-overlay-time-show.move_right";

void sync_overlay_state();

void runOnOverlay(const std::function<void(RecordingTimerOverlay *)> &fn)
{
	if (!overlay) {
		return;
	}

	QTimer::singleShot(0, overlay, [fn]() {
		if (overlay) {
			fn(overlay);
		}
	});
}

QString configFilePath()
{
	const char *path = obs_module_config_path("overlay-position.json");
	if (!path) {
		return {};
	}

	const QString result = QString::fromUtf8(path);
	bfree((void *)path);
	return result;
}

void ensure_config_dir()
{
	char *dir = obs_module_config_path("");
	if (!dir) {
		blog(LOG_WARNING, "[obs-overlay-time-show] pasta de config indisponivel");
		return;
	}

	os_mkdirs(dir);
	blog(LOG_INFO, "[obs-overlay-time-show] pasta de config: %s", dir);
	bfree(dir);
}

void load_overlay_position()
{
	if (!overlay) {
		return;
	}

	ensure_config_dir();

	const QString path = configFilePath();
	overlay->setConfigPath(path);

	if (path.isEmpty()) {
		blog(LOG_WARNING, "[obs-overlay-time-show] caminho de posicao vazio");
		return;
	}

	blog(LOG_INFO, "[obs-overlay-time-show] arquivo de posicao: %s", path.toUtf8().constData());

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_INFO, "[obs-overlay-time-show] nenhuma posicao salva ainda");
		return;
	}

	const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
	if (!obj.contains(QStringLiteral("x")) || !obj.contains(QStringLiteral("y"))) {
		return;
	}

	const QPoint position(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt());
	overlay->loadPosition(position, true);
}

void hotkey_move_up(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	runOnOverlay([](RecordingTimerOverlay *w) { w->moveBy(0, -kMoveStep); });
}

void hotkey_move_down(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	runOnOverlay([](RecordingTimerOverlay *w) { w->moveBy(0, kMoveStep); });
}

void hotkey_move_left(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	runOnOverlay([](RecordingTimerOverlay *w) { w->moveBy(-kMoveStep, 0); });
}

void hotkey_move_right(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	runOnOverlay([](RecordingTimerOverlay *w) { w->moveBy(kMoveStep, 0); });
}

obs_hotkey_id register_one_hotkey(const char *id, const char *lookup_key, obs_hotkey_func callback)
{
	const char *description = obs_module_text(lookup_key);
	obs_hotkey_id hotkey_id = obs_hotkey_register_frontend(id, description, callback, nullptr);

	if (hotkey_id == OBS_INVALID_HOTKEY_ID) {
		blog(LOG_WARNING, "[obs-overlay-time-show] falha ao registrar atalho: %s", id);
	} else {
		blog(LOG_INFO, "[obs-overlay-time-show] atalho registrado: %s (%s)", id, description);
	}

	return hotkey_id;
}

void load_hotkey_bindings(obs_hotkey_id id, const char *name)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	const char *json = config_get_string(config, "Hotkeys", name);
	if (!json || !json[0]) {
		return;
	}

	obs_data_t *data = obs_data_create_from_json(json);
	if (!data) {
		return;
	}

	obs_data_array_t *bindings = obs_data_get_array(data, "bindings");
	obs_hotkey_load(id, bindings);
	obs_data_array_release(bindings);
	obs_data_release(data);

	blog(LOG_INFO, "[obs-overlay-time-show] teclas carregadas: %s", name);
}

void save_hotkey_bindings(obs_hotkey_id id, const char *name)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}

	config_t *config = obs_frontend_get_profile_config();
	if (!config) {
		return;
	}

	obs_data_array_t *bindings = obs_hotkey_save(id);
	obs_data_t *data = obs_data_create();
	obs_data_set_array(data, "bindings", bindings);

	const char *json = obs_data_get_json(data);
	if (json) {
		config_set_string(config, "Hotkeys", name, json);
		blog(LOG_INFO, "[obs-overlay-time-show] teclas salvas: %s", name);
	}

	obs_data_release(data);
	obs_data_array_release(bindings);
}

void load_all_hotkey_bindings()
{
	load_hotkey_bindings(hotkey_up, kHotkeyNameUp);
	load_hotkey_bindings(hotkey_down, kHotkeyNameDown);
	load_hotkey_bindings(hotkey_left, kHotkeyNameLeft);
	load_hotkey_bindings(hotkey_right, kHotkeyNameRight);
}

void save_all_hotkey_bindings()
{
	save_hotkey_bindings(hotkey_up, kHotkeyNameUp);
	save_hotkey_bindings(hotkey_down, kHotkeyNameDown);
	save_hotkey_bindings(hotkey_left, kHotkeyNameLeft);
	save_hotkey_bindings(hotkey_right, kHotkeyNameRight);

	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		config_save_safe(config, "tmp", nullptr);
	}
}

void save_hotkey_array(obs_data_t *save_data, const char *key, obs_hotkey_id id)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}

	obs_data_array_t *array = obs_hotkey_save(id);
	obs_data_set_array(save_data, key, array);
	obs_data_array_release(array);
}

void load_hotkey_array(obs_data_t *save_data, const char *key, obs_hotkey_id id)
{
	if (id == OBS_INVALID_HOTKEY_ID) {
		return;
	}

	obs_data_array_t *array = obs_data_get_array(save_data, key);
	obs_hotkey_load(id, array);
	obs_data_array_release(array);
}

void on_save(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		save_hotkey_array(save_data, "move_up_hotkey", hotkey_up);
		save_hotkey_array(save_data, "move_down_hotkey", hotkey_down);
		save_hotkey_array(save_data, "move_left_hotkey", hotkey_left);
		save_hotkey_array(save_data, "move_right_hotkey", hotkey_right);
		save_all_hotkey_bindings();
	} else {
		load_hotkey_array(save_data, "move_up_hotkey", hotkey_up);
		load_hotkey_array(save_data, "move_down_hotkey", hotkey_down);
		load_hotkey_array(save_data, "move_left_hotkey", hotkey_left);
		load_hotkey_array(save_data, "move_right_hotkey", hotkey_right);
		load_all_hotkey_bindings();
	}
}

void register_hotkeys()
{
	if (hotkey_up != OBS_INVALID_HOTKEY_ID) {
		return;
	}

	hotkey_up = register_one_hotkey(kHotkeyNameUp, "OBSOverlayTimeShow.Hotkey.MoveUp", hotkey_move_up);
	hotkey_down = register_one_hotkey(kHotkeyNameDown, "OBSOverlayTimeShow.Hotkey.MoveDown", hotkey_move_down);
	hotkey_left = register_one_hotkey(kHotkeyNameLeft, "OBSOverlayTimeShow.Hotkey.MoveLeft", hotkey_move_left);
	hotkey_right = register_one_hotkey(kHotkeyNameRight, "OBSOverlayTimeShow.Hotkey.MoveRight", hotkey_move_right);
}

void unregister_hotkeys()
{
	if (hotkey_up != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_up);
		hotkey_up = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_down != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_down);
		hotkey_down = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_left != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_left);
		hotkey_left = OBS_INVALID_HOTKEY_ID;
	}
	if (hotkey_right != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(hotkey_right);
		hotkey_right = OBS_INVALID_HOTKEY_ID;
	}
}

void on_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		load_all_hotkey_bindings();
		sync_overlay_state();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED:
		load_all_hotkey_bindings();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		save_all_hotkey_bindings();
		if (overlay) {
			overlay->savePosition();
		}
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingStarted(); });
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingStopped(); });
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingPaused(); });
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingUnpaused(); });
		break;
	default:
		break;
	}
}

void sync_overlay_state()
{
	if (!overlay) {
		return;
	}

	if (obs_frontend_recording_active()) {
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingStarted(); });
		if (obs_frontend_recording_paused()) {
			runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingPaused(); });
		}
	} else {
		runOnOverlay([](RecordingTimerOverlay *w) { w->onRecordingStopped(); });
	}
}
} // namespace

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("OBSOverlayTimeShow.Description");
}

bool obs_module_load(void)
{
	overlay = new RecordingTimerOverlay();
	load_overlay_position();

	register_hotkeys();
	obs_frontend_add_save_callback(on_save, nullptr);
	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	blog(LOG_INFO, "[obs-overlay-time-show] plugin loaded");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(on_save, nullptr);
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);

	save_all_hotkey_bindings();
	unregister_hotkeys();

	if (overlay) {
		overlay->savePosition();
		overlay->deleteLater();
		overlay = nullptr;
	}

	blog(LOG_INFO, "[obs-overlay-time-show] plugin unloaded");
}
