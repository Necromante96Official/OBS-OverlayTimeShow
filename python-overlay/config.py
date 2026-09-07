"""Carrega e salva a configuracao do overlay."""

from __future__ import annotations

import json
import os
from pathlib import Path

CONFIG_NAME = "config.json"


def script_dir() -> Path:
    return Path(__file__).resolve().parent


def obs_websocket_config_paths() -> list[Path]:
    paths: list[Path] = []
    appdata = os.environ.get("APPDATA")
    if appdata:
        paths.append(
            Path(appdata) / "obs-studio" / "plugin_config" / "obs-websocket" / "config.json"
        )

    home = Path.home()
    paths.append(home / ".config" / "obs-studio" / "plugin_config" / "obs-websocket" / "config.json")
    paths.append(
        home
        / "Library"
        / "Application Support"
        / "obs-studio"
        / "plugin_config"
        / "obs-websocket"
        / "config.json"
    )
    return paths


def load_obs_websocket_password() -> str:
    for config_path in obs_websocket_config_paths():
        if not config_path.exists():
            continue
        try:
            with config_path.open(encoding="utf-8") as handle:
                data = json.load(handle)
            password = data.get("server_password", "")
            if password:
                print(f"Senha do WebSocket carregada automaticamente de {config_path}")
                return password
        except (OSError, json.JSONDecodeError):
            continue
    return ""


def resolve_password(config: dict) -> str:
    password = (config.get("password") or "").strip()
    if password:
        return password

    auto_password = load_obs_websocket_password()
    if auto_password:
        config["password"] = auto_password
        return auto_password

    return ""


def load_config() -> dict:
    config_path = script_dir() / CONFIG_NAME
    example_path = script_dir() / "config.example.json"

    if not config_path.exists():
        if example_path.exists():
            config_path.write_text(example_path.read_text(encoding="utf-8"), encoding="utf-8")
            print(f"Arquivo {CONFIG_NAME} criado a partir do exemplo.")
        else:
            raise FileNotFoundError(f"Nao encontrei {CONFIG_NAME}")

    with config_path.open(encoding="utf-8") as handle:
        config = json.load(handle)

    resolve_password(config)
    return config


def save_config(config: dict) -> None:
    config_path = script_dir() / CONFIG_NAME
    with config_path.open("w", encoding="utf-8") as handle:
        json.dump(config, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
