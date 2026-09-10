"""Janela do timer e conexao com o OBS."""

from __future__ import annotations

import sys
import threading
import time
import tkinter as tk
from tkinter import messagebox, simpledialog

try:
    import obsws_python as obs
    from obsws_python.error import OBSSDKError
except ImportError:
    print("Instale as dependencias: pip install -r requirements.txt")
    sys.exit(1)

from config import resolve_password, save_config
from windows_helpers import force_topmost, resolve_toplevel_hwnd, set_exclude_from_capture


def format_elapsed(seconds: float) -> str:
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, secs = divmod(remainder, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{secs:02d}"
    return f"{minutes:02d}:{secs:02d}"


def parse_background(value: str) -> tuple[str, float]:
    """Aceita #RRGGBB ou #AARRGGBB. Retorna (bg_hex, alpha 0..1)."""
    raw = (value or "#1A1A1A").strip()
    if not raw.startswith("#"):
        raw = "#" + raw
    hexpart = raw[1:]
    if len(hexpart) == 8:
        try:
            alpha = int(hexpart[0:2], 16) / 255.0
            rgb = f"#{hexpart[2:8].upper()}"
            return rgb, max(0.05, min(1.0, alpha))
        except ValueError:
            return "#1A1A1A", 0.9
    if len(hexpart) == 6:
        return f"#{hexpart.upper()}", 0.9
    return "#1A1A1A", 0.9


class RecordingTimerOverlay:
    def __init__(self, config: dict) -> None:
        self.config = config
        self.root = tk.Tk()
        self.root.title("OBS Recording Timer")
        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)

        bg, alpha = parse_background(config.get("background_color", "#1A1A1A"))
        fg = config.get("text_color", "#FFFFFF")
        font = (config.get("font_family", "Segoe UI"), config.get("font_size", 22), "bold")

        self.root.configure(bg=bg)
        self.root.attributes("-alpha", alpha)

        self.frame = tk.Frame(self.root, bg=bg, padx=14, pady=10)
        self.frame.pack()

        self.dot_label = tk.Label(
            self.frame, text="●", fg="#FF2D2D", bg=bg, font=(font[0], font[1] + 4, "bold")
        )
        self.time_label = tk.Label(self.frame, text="00:00", fg=fg, bg=bg, font=font)
        self.status_label = tk.Label(
            self.frame,
            text="",
            fg="#FFD166",
            bg=bg,
            font=(font[0], max(12, font[1] - 6), "bold"),
        )

        if config.get("show_rec_dot", True):
            self.dot_label.pack(side=tk.LEFT, padx=(0, 8))
        self.time_label.pack(side=tk.LEFT)
        self.status_label.pack(side=tk.LEFT, padx=(10, 0))

        self.recording = False
        self.paused = False
        self.elapsed = 0.0
        self.segment_start = 0.0
        self.lock = threading.Lock()
        self.obs_connected = False
        self.password_prompt_open = False
        self.auth_error_logged = False
        self.obs_client: obs.ReqClient | None = None
        self._password_dialog: tk.Toplevel | None = None
        self._poll_after_id: str | None = None
        self._poll_generation = 0
        self._overlay_shown = False
        self._last_status_text = ""
        self._last_displayed_second = -1
        self._capture_applied = False

        self.root.withdraw()
        self.root.after(200, self._position_window)
        self.root.after(250, self._tick)
        self._start_obs_thread()

    def _start_obs_thread(self) -> None:
        thread = threading.Thread(target=self._obs_loop, daemon=True)
        thread.start()

    def _apply_capture_exclusion(self) -> None:
        self.root.update_idletasks()
        target = resolve_toplevel_hwnd(int(self.root.winfo_id()))
        set_exclude_from_capture(target)
        self._capture_applied = True

    def _position_window(self) -> None:
        self.root.update_idletasks()
        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()
        win_w = self.root.winfo_width()
        win_h = self.root.winfo_height()
        offset_x = int(self.config.get("offset_x", 24))
        offset_y = int(self.config.get("offset_y", 24))
        position = self.config.get("position", "top-right")

        if position == "top-left":
            x, y = offset_x, offset_y
        elif position == "top-center":
            x, y = (screen_w - win_w) // 2, offset_y
        elif position == "bottom-left":
            x, y = offset_x, screen_h - win_h - offset_y
        elif position == "bottom-right":
            x, y = screen_w - win_w - offset_x, screen_h - win_h - offset_y
        elif position == "bottom-center":
            x, y = (screen_w - win_w) // 2, screen_h - win_h - offset_y
        else:
            x, y = screen_w - win_w - offset_x, offset_y

        self.root.geometry(f"+{x}+{y}")

    def _show_overlay(self) -> None:
        if self._overlay_shown:
            return
        self.root.deiconify()
        self.root.attributes("-topmost", True)
        self.root.lift()
        self.root.update_idletasks()
        target = resolve_toplevel_hwnd(int(self.root.winfo_id()))
        force_topmost(target)
        if not self._capture_applied:
            self._apply_capture_exclusion()
        self._position_window()
        self._overlay_shown = True

    def _hide_overlay(self) -> None:
        if not self._overlay_shown:
            return
        self.root.withdraw()
        self._overlay_shown = False

    def _tick(self) -> None:
        with self.lock:
            if self.recording and not self.paused:
                self.elapsed += time.monotonic() - self.segment_start
                self.segment_start = time.monotonic()
            elif self.recording and self.paused:
                self.segment_start = time.monotonic()

            displayed = int(self.elapsed)
            if displayed != self._last_displayed_second:
                self.time_label.config(text=format_elapsed(self.elapsed))
                self._last_displayed_second = displayed

            paused_text = self.config.get("paused_text", "PAUSADO")
            obs_ok = self.obs_connected

            if not obs_ok:
                status = "SEM OBS"
            elif self.paused:
                status = paused_text
            else:
                status = ""

            visible = (
                self.recording
                or not self.config.get("hide_when_not_recording", True)
                or not obs_ok
            )

        if status != self._last_status_text:
            self.status_label.config(text=status)
            self._last_status_text = status
            if visible and self._overlay_shown:
                self._position_window()

        if visible:
            self._show_overlay()
        else:
            self._hide_overlay()

        self.root.after(100, self._tick)

    def _on_record_state(self, data, seeded_seconds: float | None = None) -> None:
        active = bool(getattr(data, "output_active", False))
        paused = bool(getattr(data, "output_paused", False))

        with self.lock:
            if active and not self.recording:
                self.recording = True
                self.paused = paused
                if seeded_seconds is not None and seeded_seconds >= 0:
                    self.elapsed = float(seeded_seconds)
                else:
                    self.elapsed = 0.0
                self.segment_start = time.monotonic()
            elif active and self.recording:
                if paused and not self.paused:
                    self.elapsed += time.monotonic() - self.segment_start
                    self.paused = True
                elif not paused and self.paused:
                    self.paused = False
                    self.segment_start = time.monotonic()
            elif not active:
                self.recording = False
                self.paused = False
                self.elapsed = 0.0

    def _duration_seconds(self, status) -> float | None:
        duration = getattr(status, "output_duration", None)
        if duration is None:
            return None
        try:
            ms = float(duration)
        except (TypeError, ValueError):
            return None
        if ms < 0:
            return None
        return ms / 1000.0

    def _sync_initial_state(self, client: obs.ReqClient) -> None:
        status = client.get_record_status()

        class Data:
            pass

        data = Data()
        data.output_active = status.output_active
        data.output_paused = status.output_paused
        seeded = self._duration_seconds(status)
        self.root.after(0, lambda: self._on_record_state(data, seeded))

    def on_record_state_changed(self, data) -> None:
        """Nome exigido pelo obsws_python para o evento RecordStateChanged."""
        self.root.after(0, lambda: self._on_record_state(data))

    def _is_auth_error(self, exc: Exception) -> bool:
        if not isinstance(exc, OBSSDKError):
            return False
        message = str(exc).lower()
        return "password" in message or "identify client" in message

    def _prompt_for_password(self) -> str | None:
        if self.password_prompt_open:
            return None

        self.password_prompt_open = True
        dialog = tk.Toplevel(self.root)
        self._password_dialog = dialog
        dialog.withdraw()
        dialog.attributes("-topmost", True)

        try:
            messagebox.showinfo(
                "Senha do OBS WebSocket",
                "O OBS exige senha no WebSocket.\n\n"
                "Encontre em:\n"
                "OBS → Ferramentas → WebSocket Server Settings\n"
                "Clique em 'Show Connect Info' para ver a senha.",
                parent=dialog,
            )
            if not self.password_prompt_open:
                return None
            password = simpledialog.askstring(
                "Senha do OBS WebSocket",
                "Digite a senha do WebSocket:",
                show="*",
                parent=dialog,
            )
            return password.strip() if password else None
        finally:
            self._password_dialog = None
            try:
                dialog.destroy()
            except tk.TclError:
                pass
            self.password_prompt_open = False

    def _cancel_password_prompt(self) -> None:
        self.password_prompt_open = False
        dialog = self._password_dialog
        self._password_dialog = None
        if dialog is not None:
            try:
                dialog.destroy()
            except tk.TclError:
                pass

    def _wait_for_password(self) -> str | None:
        event = threading.Event()
        result: list[str | None] = [None]

        def ask() -> None:
            result[0] = self._prompt_for_password()
            event.set()

        self.root.after(0, ask)
        if not event.wait(timeout=300):
            self.root.after(0, self._cancel_password_prompt)
            return None
        return result[0]

    def _cancel_poll(self) -> None:
        self._poll_generation += 1
        if self._poll_after_id is not None:
            try:
                self.root.after_cancel(self._poll_after_id)
            except Exception:
                pass
            self._poll_after_id = None

    def _schedule_poll(self, generation: int) -> None:
        def tick() -> None:
            self._poll_after_id = None
            if generation != self._poll_generation:
                return
            self._poll_record_status(generation)

        self._poll_after_id = self.root.after(2000, tick)

    def _poll_record_status(self, generation: int) -> None:
        if generation != self._poll_generation:
            return

        client = self.obs_client
        if client is not None:
            try:
                status = client.get_record_status()

                class Data:
                    pass

                data = Data()
                data.output_active = status.output_active
                data.output_paused = status.output_paused
                seeded = self._duration_seconds(status)
                # So semeia duration no sync inicial / reconnect; em poll so
                # atualiza estado. Se ainda nao estamos gravando localmente,
                # passa duration para alinhar mid-recording.
                with self.lock:
                    already = self.recording
                self._on_record_state(data, None if already else seeded)
            except Exception as exc:
                print(f"Falha ao consultar status de gravacao: {exc}")
                self.obs_connected = False
                self.obs_client = None
                return

        if self.obs_connected:
            self._schedule_poll(generation)

    def _start_poll_chain(self) -> None:
        self._cancel_poll()
        generation = self._poll_generation
        self._poll_record_status(generation)

    def _disconnect_clients(self, req, events) -> None:
        self._cancel_poll()
        self.obs_connected = False
        self.obs_client = None
        if events is not None:
            try:
                events.disconnect()
            except Exception:
                pass
        if req is not None:
            try:
                disconnect = getattr(req, "disconnect", None)
                if callable(disconnect):
                    disconnect()
                else:
                    base = getattr(req, "base_client", None)
                    ws = getattr(base, "ws", None) if base is not None else None
                    if ws is not None:
                        ws.close()
            except Exception:
                pass

    def _obs_loop(self) -> None:
        host = self.config.get("host", "127.0.0.1")
        port = int(self.config.get("port", 4455))

        while True:
            password = (self.config.get("password") or "").strip()
            if not password:
                password = resolve_password(self.config)

            req = None
            events = None
            try:
                req = obs.ReqClient(host=host, port=port, password=password, timeout=5)
                events = obs.EventClient(host=host, port=port, password=password)
                events.callback.register(self.on_record_state_changed)
                self.obs_client = req
                self.obs_connected = True
                self.auth_error_logged = False
                print("Conectado ao OBS WebSocket.")
                self._sync_initial_state(req)
                self.root.after(0, self._start_poll_chain)

                while self.obs_connected:
                    try:
                        req.get_version()
                    except Exception:
                        self.obs_connected = False
                        break
                    time.sleep(2)
            except Exception as exc:
                if self._is_auth_error(exc):
                    if not self.auth_error_logged:
                        print("Falha de autenticacao no OBS WebSocket. Informe a senha.")
                        self.auth_error_logged = True

                    self._disconnect_clients(req, events)
                    new_password = self._wait_for_password()
                    if new_password:
                        self.config["password"] = new_password
                        save_config(self.config)
                        print("Senha salva em config.json.")
                        continue

                    time.sleep(5)
                    continue

                if not self.auth_error_logged or "Connection refused" not in str(exc):
                    print(f"Aguardando OBS WebSocket... ({exc})")
            finally:
                self._disconnect_clients(req, events)

            time.sleep(3)

    def run(self) -> None:
        self.root.mainloop()
