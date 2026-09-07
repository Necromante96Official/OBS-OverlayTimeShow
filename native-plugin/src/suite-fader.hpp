#pragma once

#include <QString>

// Faz transicoes de audio (fade in / fade out) sem travar a interface do OBS.
// Todas as funcoes devem ser chamadas na thread da interface.
namespace SuiteFader {

struct FadeRequest {
  // Cena usada apenas quando hideItemWhenDone esta ligado.
  QString sceneName;
  QString sourceName;

  // Volume final, de 0.0 (sem som) a 1.0 (volume cheio).
  double targetVolume = 1.0;
  int durationMs = 500;

  // Volume inicial: quando negativo, usa o volume atual da fonte.
  double startVolume = -1.0;

  bool unmuteAtStart = false;
  bool hideItemWhenDone = false;
  bool restoreVolumeWhenDone = false;
};

// Inicia o fade. Um novo fade na mesma fonte substitui o anterior.
// Retorna false quando a fonte nao existe.
bool startFade(const FadeRequest &request);

// Interrompe os fades em andamento, aplicando o volume final de cada um.
void finishAllNow();

// Interrompe os fades em andamento e deixa o volume onde estava.
void cancelAll();

} // namespace SuiteFader
