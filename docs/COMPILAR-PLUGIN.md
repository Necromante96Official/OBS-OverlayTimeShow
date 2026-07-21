# Como compilar o plugin nativo do OBS

Este guia e para quem quer o timer **dentro do OBS**, sem Python.

## O que voce precisa

1. Windows 10/11
2. Visual Studio 2022 com "Desenvolvimento para Desktop com C++"
3. CMake 3.28 ou superior
4. Git

## Passo a passo resumido

1. Na pasta do projeto, execute:

```bat
scripts\setup-plugin.bat
```

2. Siga a documentacao do [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) para configurar o build (CMake presets com Frontend API e Qt ativos).

3. Compile e instale:

```bat
scripts\rebuild-plugin.bat
```

4. Feche e abra o OBS. Inicie uma gravacao para ver o timer.

## Instalacao manual da DLL

Se a instalacao automatica nao achar o OBS:

1. Copie `build-output\obs-overlay-time-show.dll` para:

```
<pasta-do-OBS>\obs-plugins\64bit\
```

2. Copie os arquivos de `native-plugin\data\locale\` para:

```
<pasta-do-OBS>\data\obs-plugins\obs-overlay-time-show\locale\
```

Voce tambem pode definir a variavel de ambiente `OBS_STUDIO_PATH` apontando para a pasta do OBS.

## Atalhos

No OBS: **Configuracoes → Atalhos** e busque `Timer OBS` para mover o overlay.

## Alternativa sem compilar

Use o overlay Python:

```bat
Iniciar-Overlay.bat
```
