"""Ponto de entrada do overlay de gravacao."""

from __future__ import annotations

from config import load_config
from overlay import RecordingTimerOverlay


def main() -> None:
    config = load_config()
    app = RecordingTimerOverlay(config)
    app.run()


if __name__ == "__main__":
    main()
