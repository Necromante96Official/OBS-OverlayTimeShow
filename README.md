# OBS Overlay Time Show

Timer de gravacao para OBS Studio, no estilo do indicador do **Action! (Mirillis)**: voce ve o tempo na tela enquanto grava, mas o overlay **nao entra no video gravado**.

## O que este projeto faz

- Inicia a contagem quando a gravacao comeca
- Pausa ao pausar a gravacao no OBS
- Retoma ao despausar
- Para e zera ao encerrar a gravacao
- Exibe um indicador visual (ponto vermelho + tempo)
- **Nao aparece na gravacao** usando a API do Windows `SetWindowDisplayAffinity` (Windows 10 2004+ / Windows 11)

## Requisitos

- Windows 10 (2004 ou superior) ou Windows 11
- [OBS Studio](https://obsproject.com/) 28+
- Python 3.10+ (para o overlay na tela)
- **OBS WebSocket** ativado:
  1. OBS → **Ferramentas** → **WebSocket Server Settings**
  2. Marque **Enable WebSocket server**
  3. Anote a porta (padrao `4455`) e a senha, se houver

## Instalacao rapida (overlay na tela)

1. Clone ou baixe este repositorio
2. Execute `start-overlay.bat`
3. Na primeira execucao, sera criado `config.json` a partir de `config.example.json`
4. Edite `config.json` e coloque a senha do WebSocket, se voce configurou uma
5. Inicie uma gravacao no OBS — o overlay deve aparecer no canto da tela

### Configuracao (`config.json`)

| Campo | Descricao |
|-------|-----------|
| `host` | Endereco do OBS (padrao `127.0.0.1`) |
| `port` | Porta WebSocket (padrao `4455`) |
| `password` | Senha do WebSocket |
| `position` | `top-right`, `top-left`, `top-center`, `bottom-right`, `bottom-left`, `bottom-center` |
| `offset_x`, `offset_y` | Margem em pixels |
| `font_family`, `font_size` | Fonte do timer |
| `text_color`, `background_color` | Cores (hex) |
| `show_rec_dot` | Mostrar ponto vermelho de gravacao |
| `hide_when_not_recording` | Esconder overlay quando nao estiver gravando |

## Plugin compilado e instalado

O plugin nativo foi compilado e copiado para o OBS (Steam):

```
D:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\obs-overlay-time-show.dll
```

**Para usar:** feche e reabra o OBS, depois inicie uma gravacao.

### Atalhos para mover o timer

Configure em **OBS → Configuracoes → Atalhos** e pesquise por **`Timer OBS`**:

| Atalho na lista | Acao |
|-----------------|------|
| Timer OBS: mover para cima | Move 24 px |
| Timer OBS: mover para baixo | Move 24 px |
| Timer OBS: mover para esquerda | Move 24 px |
| Timer OBS: mover para direita | Move 24 px |

> Se nao aparecer, feche o OBS e rode `install-plugin.bat` (a versao antiga do plugin nao tinha atalhos).

A posicao e salva automaticamente entre sessoes.

### Visual compacto

- **Verde ●** = gravando
- **Amarelo ⏸** = pausado
- Apenas icone + tempo (sem texto "PAUSADO")

Copia de backup do DLL: `dist/obs-overlay-time-show.dll`
Reinstalar depois de atualizar: `install-plugin.bat`

### Recompilar (se necessario)

```bat
cd obs-plugintemplate-build
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
install-plugin.bat
```

---

## Alternativa: plugin nativo do OBS (sem Python)

O codigo do plugin esta em `obs-plugin/`. Ele faz o mesmo que o overlay Python, mas roda **dentro do OBS** (sem WebSocket).

**Vantagens:** um unico programa, inicia com o OBS, sem senha WebSocket.
**Desvantagem:** precisa **compilar** o plugin (Visual Studio + CMake + fontes do OBS).

Guia completo: [`obs-plugin/PLUGIN-BUILD.md`](obs-plugin/PLUGIN-BUILD.md)
Script auxiliar: `setup-plugin.bat`

---

## Alternativa: dock interno do OBS (sem Python)

Se preferir o timer **dentro da interface do OBS** (nunca aparece na gravacao):

1. Ative o WebSocket no OBS
2. Abra `obs-dock/timer.html` e ajuste `WS_PASSWORD` se necessario
3. OBS → **Docks** → **Custom Browser Docks...**
4. Nome: `Timer de Gravacao`
5. URL: caminho completo do arquivo, por exemplo:
   `file:///D:/OBS-OverlayTimeShow/obs-dock/timer.html`
6. Marque o dock em **Docks** no menu **View**

Esse modo nao flutua sobre o jogo; fica no painel do OBS.

## Por que funciona sem aparecer na gravacao?

O overlay na tela usa `WDA_EXCLUDEFROMCAPTURE`, a mesma ideia de overlays modernos no Windows: a janela fica visivel para voce, mas e ignorada por capturas de tela e pelo OBS ao gravar a area de trabalho.

**Importante:** isso funciona com **Captura de Tela/Display Capture** e gravacao normal do OBS. Se voce adicionar o timer como **fonte dentro da cena** (Browser Source, Texto, etc.), ele **vai** aparecer na gravacao.

## Solucao de problemas

| Problema | O que verificar |
|----------|-----------------|
| Overlay nao conecta | OBS aberto, WebSocket ativo, senha correta em `config.json` |
| Erro `authentication enabled but no password` | O script tenta ler a senha automaticamente do OBS. Se falhar, abra `config.json` e preencha `"password"` com a senha de **Ferramentas → WebSocket Server Settings → Show Connect Info** |
| Overlay aparece na gravacao | Windows desatualizado; atualize para 10 2004+ ou use o dock interno |
| Timer nao pausa | Use OBS 28+ com suporte a pausar gravacao |
| Python nao encontrado | Instale em https://www.python.org/ e marque "Add to PATH" |

## Estrutura

```
OBS-OverlayTimeShow/
  overlay/recording_timer.py   # Overlay na tela (principal)
  obs-dock/timer.html          # Timer no dock do OBS (alternativa)
  config.example.json
  start-overlay.bat
  requirements.txt
```

## Licenca

Uso livre para projetos pessoais e streams.
