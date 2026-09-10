# OBS Overlay Time Show

Timer de gravação estilo Action! que **não aparece na captura**, mais automações (Suites) no plugin nativo.

Requer Windows 10/11 e OBS Studio 28+ (plugin testado com ~31.1.1).

## Modos

| Modo | Quando usar | Como iniciar |
|------|-------------|--------------|
| **Plugin nativo** (recomendado) | Timer + Suites + cantos da câmera + filtro de cantos | `Setup-Build-Env.bat` uma vez, depois `Build-Install-Plugin.bat` |
| **Overlay Python** | Só o timer, sem instalar plugin | `python-overlay\start.bat` (WebSocket do OBS ligado) |
| **Browser dock** | Timer como dock interno do OBS | `Ferramentas → Docks do navegador personalizados` apontando para `browser-dock\timer.html` |

### Aviso do browser dock

Use como **Custom Browser Dock**, não como Browser Source. Em Browser Source o timer entra no vídeo. Preencha `WS_PASSWORD` em `timer.html` se o WebSocket tiver senha.

## Plugin nativo

1. Rode `Setup-Build-Env.bat` (MSVC, CMake, deps).
2. Rode `Build-Install-Plugin.bat` (fecha o OBS antes).
3. O instalador procura OBS em `OBS_STUDIO_PATH`, Steam (`D:\SteamLibrary\...`) e Program Files.

No OBS: dock **Suites**, atalhos para mover o timer / encerrar com outro / câmera.

## Overlay Python

1. Em OBS: **Ferramentas → Configurações do servidor WebSocket** → ativar.
2. Copie `python-overlay\config.example.json` para `config.json` se quiser ajustar.
3. `python-overlay\start.bat`

A senha do WebSocket pode ficar em `config.json` (texto local). Prefira localhost.

`background_color` aceita `#RRGGBB` ou `#AARRGGBB` (alpha + RGB).

## Licença / build

Fontes do plugin: `native-plugin\`. Artefatos: `build-output\` / `dist\`.
