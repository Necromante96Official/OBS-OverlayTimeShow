#pragma once

#include <QString>
#include <QStringList>

// Move uma fonte (tipicamente a camera) para um canto da tela. O
// posicionamento usa o alinhamento do item da cena, entao funciona sem
// precisar saber o tamanho nem a escala da fonte.
namespace CameraPosition {

enum class Anchor {
  TopLeft,
  TopCenter,
  TopRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
};

QString anchorConfigName(Anchor anchor);
QString anchorLabel(Anchor anchor);

// Fonte que os atalhos movem, distancia da borda e se vale para todas as cenas.
// Sem nada escolhido, sourceName() adota a camera que encontrar.
QString sourceName();
QString guessSourceName();
void setSourceName(const QString &name);
int margin();
void setMargin(int pixels);
bool allScenes();
void setAllScenes(bool enabled);

// Fontes de video existentes, para o seletor do painel.
QStringList videoSources();

// Move a fonte configurada. false quando ela nao esta na cena.
bool apply(Anchor anchor);

// Canto onde a fonte esta agora, lido do alinhamento do item da cena.
bool currentAnchor(Anchor *anchor);

// Anda um passo na rota dos cantos: 1 segue no sentido do relogio, -1 volta.
bool cycle(int direction);

void load();
void save();

} // namespace CameraPosition
