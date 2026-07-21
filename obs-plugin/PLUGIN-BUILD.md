# Plugin OBS — Overlay Time Show

Plugin nativo para OBS Studio com o mesmo comportamento do overlay Python:

- Timer visivel na sua tela durante a gravacao
- **Nao aparece no video gravado** (`WDA_EXCLUDEFROMCAPTURE` no Windows)
- Pausa e retoma com a gravacao do OBS
- Some ao parar a gravacao
- Sem Python, sem WebSocket, sem processo externo

## Comparacao

| Recurso | Python (`start-overlay.bat`) | Plugin OBS |
|---------|------------------------------|------------|
| Timer na tela | Sim | Sim |
| Excluido da gravacao | Sim | Sim |
| Pausa/retoma | Sim | Sim |
| Instalacao | Python + WebSocket | Compilar + copiar DLL |
| Integracao OBS | Externa | Nativa |

## Requisitos para compilar (Windows)

1. **Visual Studio 2022** — carga de trabalho "Desenvolvimento para Desktop com C++"
2. **CMake 3.28+**
3. **Git**
4. **Codigo-fonte do OBS Studio** (para headers e libs de desenvolvimento)

### Obter o codigo-fonte do OBS

```powershell
git clone --recursive https://github.com/obsproject/obs-studio.git
cd obs-studio
cmake --preset windows-x64
cmake --build --preset windows-x64 --target obs-frontend-api
```

Ou aponte para uma instalacao de desenvolvimento ja existente no `CMakeUserPresets.json`.

## Compilar o plugin

### Opcao A — script auxiliar

Na raiz do projeto:

```bat
setup-plugin.bat
```

Isso clona o [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) e copia os arquivos de `obs-plugin/`.

### Opcao B — manual

1. Clone o template:

```bat
git clone https://github.com/obsproject/obs-plugintemplate.git
cd obs-plugintemplate
```

2. Substitua/copie de `obs-plugin/`:
   - `src/`
   - `data/`
   - `buildspec.json`
   - `CMakeLists.txt`

3. Em `CMakePresets.json`, confirme:

```json
"ENABLE_FRONTEND_API": true,
"ENABLE_QT": true
```

4. Crie `CMakeUserPresets.json` com o caminho do OBS (exemplo):

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "windows-x64-user",
      "inherits": "windows-x64",
      "cacheVariables": {
        "OBS_DIR": "C:/dev/obs-studio"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows-x64-user",
      "configurePreset": "windows-x64-user"
    }
  ]
}
```

5. Compile:

```bat
cmake --preset windows-x64-user
cmake --build --preset windows-x64-user
```

## Instalar no OBS

Copie a pasta gerada em `build_x64/` (ou `rundir`) para:

```
%ProgramFiles%\obs-studio\obs-plugins\64bit\
```

E os dados de locale para:

```
%ProgramFiles%\obs-studio\data\obs-plugins\obs-overlay-time-show\
```

Reinicie o OBS. O plugin carrega automaticamente — nao precisa adicionar fonte na cena.

## Estrutura do codigo

```
obs-plugin/
  src/plugin-main.cpp           # Eventos OBS (gravar/pausar/parar)
  src/recording-timer-overlay.* # Janela Qt flutuante + timer
  data/locale/                  # Textos pt-BR e en-US
  CMakeLists.txt
  buildspec.json
```

## Limitacoes atuais

- **Windows apenas** para exclusao da captura (API `SetWindowDisplayAffinity`)
- Posicao fixa no canto superior direito (configuravel no codigo)
- Timer zera ao iniciar nova gravacao (igual ao overlay Python)
- Requer compilacao — nao ha instalador pronto ainda

## Solucao de problemas

| Problema | Solucao |
|----------|---------|
| Erro LNK2019 `obs_frontend_*` | Ative `ENABLE_FRONTEND_API` no CMake |
| Plugin nao aparece | Confirme DLL em `obs-plugins\64bit\` |
| Overlay nao visivel | Gravacao precisa estar ativa |
| Overlay aparece na gravacao | Atualize Windows 10 2004+ / 11 |

## Pre-built

Se preferir nao compilar, continue usando `start-overlay.bat` — o comportamento e o mesmo apos a correcao do WebSocket.
