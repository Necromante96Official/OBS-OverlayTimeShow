#pragma once

#include "suite-types.hpp"

#include <QPair>
#include <QStringList>
#include <QVector>

namespace SuiteActions {

QStringList sceneNames();
QStringList sourceNamesInScene(const QString &sceneName);
QStringList transitionNames();
QStringList audioSourceNames();

// Tipos de transicao que o OBS oferece para mostrar/ocultar uma fonte,
// em pares de identificador interno e nome que aparece na tela.
QVector<QPair<QString, QString>> itemTransitionTypes();

// Diz se a fonte tem canal de audio (serve para liberar as opcoes de fade).
bool sourceHasAudio(const QString &sourceName);

bool setScene(const QString &sceneName);
// transitionId vazio nao mexe na transicao da fonte; "none" tira a transicao;
// qualquer outro valor e um tipo de transicao do OBS.
bool setSourceVisible(const QString &sceneName, const QString &sourceName,
                      bool visible, const QString &transitionId = QString(),
                      int transitionMs = 300);
// Prepara a transicao de mostrar (show = true) ou ocultar (show = false) da
// fonte, sem mexer na visibilidade agora.
bool applyItemTransition(const QString &sceneName, const QString &sourceName,
                         bool show, const QString &transitionId,
                         int transitionMs);
bool setTransition(const QString &transitionName, int durationMs);
bool setMute(const QString &sourceName, bool muted);
bool setVolume(const QString &sourceName, double volume);
bool restartMedia(const QString &sourceName);
bool openUrl(const QString &url);
bool openRecordingFolder();

// Escurece a tela toda ate o preto (fadeFromBlack = false) ou sai do preto
// mostrando a imagem (fadeFromBlack = true), usando uma camada preta do
// tamanho da tela criada na cena.
bool screenFade(const QString &sceneName, bool fadeFromBlack, int durationMs);

bool isRecording();
bool stopRecording();

bool isCurrentScene(const QString &sceneName);
bool isSourceVisible(const QString &sceneName, const QString &sourceName,
                     bool expectVisible);

// Executa um passo. skipNextOut avisa que o passo seguinte deve ser ignorado
// (condicoes) e waitMsOut informa quanto tempo esperar antes do proximo passo
// (transicoes de audio que precisam terminar primeiro).
bool executeStep(const SuiteStep &step, bool *skipNextOut,
                 int *waitMsOut = nullptr);

} // namespace SuiteActions
