"""Funcoes do Windows para overlay: fora da captura e sempre no topo."""

from __future__ import annotations

from ctypes import windll, wintypes

WDA_EXCLUDEFROMCAPTURE = 0x00000011
GWL_EXSTYLE = -20
WS_EX_TOPMOST = 0x00000008
WS_EX_TOOLWINDOW = 0x00000080
WS_EX_NOACTIVATE = 0x08000000
HWND_TOPMOST = wintypes.HWND(-1)
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010
SWP_SHOWWINDOW = 0x0040
SWP_ASYNCWINDOWPOS = 0x4000

user32 = windll.user32
kernel32 = windll.kernel32
user32.SetWindowPos.argtypes = [
    wintypes.HWND,
    wintypes.HWND,
    wintypes.INT,
    wintypes.INT,
    wintypes.INT,
    wintypes.INT,
    wintypes.UINT,
]
user32.SetWindowPos.restype = wintypes.BOOL
user32.GetWindowLongW.argtypes = [wintypes.HWND, wintypes.INT]
user32.GetWindowLongW.restype = wintypes.LONG
user32.SetWindowLongW.argtypes = [wintypes.HWND, wintypes.INT, wintypes.LONG]
user32.SetWindowLongW.restype = wintypes.LONG


def set_exclude_from_capture(hwnd: int) -> bool:
    ok = bool(user32.SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE))
    if not ok:
        print("Aviso: nao foi possivel excluir o overlay da captura. Atualize o Windows 10/11.")
    return ok


def force_topmost(hwnd: int) -> bool:
    """Mantem a janela acima das outras sem roubar o foco."""
    target = wintypes.HWND(hwnd)
    style = int(user32.GetWindowLongW(target, GWL_EXSTYLE))
    new_style = style | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE
    user32.SetWindowLongW(target, GWL_EXSTYLE, new_style)

    flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_ASYNCWINDOWPOS
    return bool(user32.SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, flags))


def resolve_toplevel_hwnd(child_hwnd: int) -> int:
    parent = user32.GetParent(child_hwnd)
    return int(parent or child_hwnd)
