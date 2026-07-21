# OBS Overlay Time Show

Timer de gravacao para o OBS Studio, no estilo do indicador do Action!.

Voce ve o tempo na tela enquanto grava. O overlay **nao entra no video**.

---

## O que ele faz

- Comeca a contar quando a gravacao inicia
- Pausa e retoma junto com o OBS
- Para e some quando a gravacao termina
- Pode ser movido com o mouse (plugin) ou por atalhos
- Nao aparece na gravacao (Windows 10 2004+ / Windows 11)

---

## Qual modo usar?

| Modo | Para quem | Como comecar |
|------|-----------|--------------|
| **Overlay Python** (mais facil) | Qualquer pessoa | Duplo clique em `Iniciar-Overlay.bat` |
| **Plugin nativo** | Quem quer tudo dentro do OBS | Compilar: veja `docs/COMPILAR-PLUGIN.md` |
| **Dock no OBS** | Quem so quer ver o tempo no painel do OBS | Use `browser-dock/timer.html` |

Para a maioria das pessoas, o **Overlay Python** e o caminho mais simples.

---

## Inicio rapido (recomendado)

### 1. Requisitos

- Windows 10 (atualizado) ou Windows 11
- [OBS Studio](https://obsproject.com/) 28 ou superior
- [Python 3.10+](https://www.python.org/downloads/)
  Na instalacao, marque **Add python.exe to PATH**

### 2. Ativar o WebSocket no OBS

1. Abra o OBS
2. Menu **Ferramentas** → **WebSocket Server Settings**
3. Marque **Enable WebSocket server**
4. Se houver senha, anote (ou deixe o programa pedir depois)

### 3. Rodar o overlay

1. Baixe ou clone este projeto
2. De duplo clique em **`Iniciar-Overlay.bat`**
3. Na primeira vez, ele cria um ambiente Python e instala o necessario
4. Inicie uma **gravacao** no OBS
5. O timer deve aparecer no canto da tela

### 4. Senha do WebSocket (se pedir)

Se o OBS tiver autenticacao ligada:

1. OBS → **Ferramentas** → **WebSocket Server Settings** → **Show Connect Info**
2. Copie a senha
3. Cole quando o overlay pedir
   ou edite `python-overlay/config.json` e preencha `"password"`

> O arquivo `config.json` fica **so no seu computador**. Ele nao deve ser enviado para o GitHub.

---

## Configuracao basica (Python)

Arquivo de exemplo: `python-overlay/config.example.json`
Arquivo real (criado automaticamente): `python-overlay/config.json`

Campos uteis:

| Campo | Significado |
|-------|-------------|
| `password` | Senha do WebSocket (deixe vazio se nao usar) |
| `position` | Posicao: `top-right`, `top-left`, `bottom-right`, etc. |
| `offset_x` / `offset_y` | Distancia das bordas |
| `hide_when_not_recording` | Esconde o timer quando nao esta gravando |

---

## Plugin nativo (opcional)

O plugin fica em `native-plugin/` e roda dentro do OBS (sem Python e sem WebSocket).

- Visual em formato “pill” (HUD)
- Ponto verde pulsando ao gravar
- Arrastar com o mouse
- Atalhos para mover

Guia completo: [`docs/COMPILAR-PLUGIN.md`](docs/COMPILAR-PLUGIN.md)

Atalhos no OBS: **Configuracoes → Atalhos** → busque **Timer OBS**.

---

## Dock interno (opcional)

1. Ative o WebSocket no OBS
2. Se precisar, edite a senha em `browser-dock/timer.html` (`WS_PASSWORD`)
3. OBS → **Docks** → **Custom Browser Docks...**
4. Nome: `Timer de Gravacao`
5. URL: caminho `file:///` ate `browser-dock/timer.html`

Esse modo fica no painel do OBS (nao flutua sobre o jogo).

---

## Por que nao aparece na gravacao?

No Windows, o overlay usa uma funcao do sistema que **exclui a janela da captura**.

Isso vale para Captura de Tela / Display Capture.
Se voce adicionar o timer como **fonte na cena** (Browser Source, Texto, etc.), ele **vai** aparecer no video.

---

## Problemas comuns

| Problema | O que fazer |
|----------|-------------|
| Overlay nao conecta | OBS aberto, WebSocket ligado, senha correta |
| Pediu senha | Use Show Connect Info no OBS e preencha `config.json` |
| Overlay aparece no video | Atualize o Windows; nao use o timer como fonte da cena |
| Python nao encontrado | Reinstale o Python marcando Add to PATH |
| Plugin nao aparece | Feche o OBS e rode `scripts\install-plugin.bat` |

---

## Estrutura do projeto

```
OBS-OverlayTimeShow/
├── Iniciar-Overlay.bat      ← comece por aqui
├── README.md
├── .gitignore
├── python-overlay/          ← modo facil (Python)
├── native-plugin/           ← codigo do plugin OBS
├── browser-dock/            ← timer no painel do OBS
├── scripts/                 ← instalar / compilar / limpar / GitHub
└── docs/                    ← guias extras
```

Arquivos antigos (`overlay/`, `obs-plugin/`, `obs-dock/`) nao devem ficar na raiz.
Se ainda existirem apos atualizar, rode uma vez: `scripts\limpar-legado.bat`

---

## Privacidade

- Nao compartilhe seu `config.json` (pode ter senha do WebSocket)
- Nao compartilhe pastas `.venv/`, `build-output/` ou caminhos pessoais do seu PC
- Este README usa apenas exemplos genericos

---

## Licenca

Uso livre para projetos pessoais e streams.
