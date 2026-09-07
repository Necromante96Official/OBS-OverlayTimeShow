#pragma once

#include "suite-types.hpp"

#include <QStringList>

namespace SuiteActions {

QStringList sceneNames();
QStringList sourceNamesInScene(const QString &sceneName);
QStringList transitionNames();
QStringList audioSourceNames();

// Diz se a fonte tem canal de audio (serve para liberar as opcoes de fade).
bool sourceHasAudio(const QString &sourceName);

bool setScene(const QString &sceneName);
bool setSourceVisible(const QString &sceneName, const QString &sourceName,
                      bool visible);
bool setTransition(const QString &transitionName, int durationMs);
bool setMute(const QString &sourceName, bool muted);
bool setVolume(const QString &sourceName, double volume);
bool restartMedia(const QString &sourceName);
bool openUrl(const QString &url);
bool openRecordingFolder();

bool isCurrentScene(const QString &sceneName);
bool isSourceVisible(const QString &sceneName, const QString &sourceName,
                     bool expectVisible);

// Executa um passo. skipNextOut avisa que o passo seguinte deve ser ignorado
// (condicoes) e waitMsOut informa quanto tempo esperar antes do proximo passo
// (transicoes de audio que precisam terminar primeiro).
bool executeStep(const SuiteStep &step, bool *skipNextOut,
                 int *waitMsOut = nullptr);

} // namespace SuiteActions
