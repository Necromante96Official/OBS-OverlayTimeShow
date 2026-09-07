# OBS Overlay Time Show

Timer de gravacao para o OBS Studio, no estilo do indicador do Action!.

Voce ve o tempo na tela enquanto grava. O overlay **nao entra no video**.

---

## O que ele faz

- Timer de gravacao na tela (nao entra no video)
- Dock **Suites de Automacao**: perfis por jogo (so uma ativa)
- Ao iniciar a gravacao, executa os passos da suite ativa
- Ao terminar, pode abrir a pasta do arquivo gravado
- Pode ser movido com o mouse ou por atalhos
- Atalho para executar a suite ativa sem gravar

---

## Suites (painel no OBS)

1. No OBS: **Docks** / painel **Suites de Automacao**
2. Crie uma suite (ex.: Necesse), clique **Ativar**
3. Marque **Ao terminar, abrir pasta da gravacao** se quiser
4. Adicione passos: cena, fonte, delay, transicao, mute, volume, condicoes, etc.
5. Grave — a suite ativa roda sozinha

Atalho: **Configuracoes → Atalhos** → **Suites: executar suite ativa**

Configs das suites ficam no AppData do plugin (`suites.json`), preservadas entre atualizacoes.

---

## Inicio rapido (plugin nativo)

### 1. Requisitos

- Windows 10 (atualizado) ou Windows 11
- OBS Studio (Steam): `D:\SteamLibrary\steamapps\common\OBS Studio`

Na primeira vez, rode **`Setup-Build-Env.bat`**. Ele instala e coloca no PATH:

- CMake
- Git (se faltar)
- Visual Studio 2022 Build Tools (C++)

Depois feche e reabra o terminal/Cursor para o PATH valer em todos os programas.

### 2. Compilar e instalar

1. Feche o OBS
2. De duplo clique em **`Build-Install-Plugin.bat`**
3. Espere a compilacao e a copia para a pasta do OBS
4. Abra o OBS e inicie uma **gravacao**

O script:

- Compila o plugin
- Remove a DLL/textos antigos do plugin
- Instala a versao nova
- **Nao apaga** posicao do timer, atalhos, cenas ou perfis do OBS

### 3. Destino da instalacao

```
D:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\obs-overlay-time-show.dll
D:\SteamLibrary\steamapps\common\OBS Studio\data\obs-plugins\obs-overlay-time-show\
```

Para outro caminho do OBS, defina a variavel `OBS_STUDIO_PATH` ou edite o caminho no `.bat`.

Atalhos no OBS: **Configuracoes → Atalhos** → busque **Timer OBS**.

---

## Outros modos (opcionais)

| Modo | Quando usar |
|------|-------------|
| **Overlay Python** | Teste rapido sem compilar: `python-overlay\start.bat` |
| **Dock no OBS** | So ver o tempo no painel: `browser-dock\timer.html` |

---

## Overlay Python (opcional)

1. OBS → **Ferramentas** → **WebSocket Server Settings** → ative o servidor
2. Rode `python-overlay\start.bat`
3. Na primeira vez ele cria o ambiente e instala as dependencias

Config: `python-overlay/config.example.json` → `python-overlay/config.json`

---

## Dock interno (opcional)

1. Ative o WebSocket no OBS
2. Se precisar, edite a senha em `browser-dock/timer.html` (`WS_PASSWORD`)
3. OBS → **Docks** → **Custom Browser Docks...**
4. Nome: `Timer de Gravacao`
5. URL: caminho `file:///` ate `browser-dock/timer.html`

---

## Por que nao aparece na gravacao?

No Windows, o overlay usa uma funcao do sistema que **exclui a janela da captura**.

Isso vale para Captura de Tela / Display Capture.
Se voce adicionar o timer como **fonte na cena**, ele **vai** aparecer no video.

---

## Problemas comuns

| Problema | O que fazer |
|----------|-------------|
| Build falhou | Rode `Setup-Build-Env.bat`, feche/abra o terminal e tente de novo |
| OBS esta aberto | Feche o OBS e rode o `.bat` de novo |
| Plugin nao aparece | Confirme a pasta Steam do OBS e reinicie o OBS |
| Overlay aparece no video | Atualize o Windows; nao use o timer como fonte da cena |

---

## Estrutura do projeto

```
OBS-OverlayTimeShow/
├── Setup-Build-Env.bat        ← primeira vez (PATH + ferramentas)
├── Build-Install-Plugin.bat   ← compilar e instalar no OBS
├── README.md
├── native-plugin/             ← codigo do plugin OBS
├── python-overlay/            ← modo opcional (Python)
├── browser-dock/              ← timer no painel do OBS
└── scripts/                   ← setup / PATH / instalar / limpar
```

---

## Privacidade

- Nao compartilhe pastas `.venv/`, `build-output/` ou caminhos pessoais do seu PC
- Posicao do timer fica no AppData do OBS (`plugin_config`), fora da pasta Steam

---

## Licenca

Uso livre para projetos pessoais e streams.
