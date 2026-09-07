#pragma once

#include <QString>

// A tecla de "Parar gravacao" do OBS encerra o arquivo na hora, sem dar tempo
// de escurecer a tela. Estas funcoes permitem passar essa mesma tecla para o
// encerramento suave do plugin, e devolve-la depois.
namespace SuiteOutroHotkey {

// Tecla que hoje para a gravacao no OBS, em texto legivel ("Num3"). Vazio
// quando o atalho do OBS esta sem tecla.
QString obsStopKeyText();

// Tecla que hoje aciona o encerramento suave. Vazio quando nao tem tecla.
QString outroKeyText();

// Passa a tecla do OBS para o encerramento suave. Devolve false e preenche
// message com o motivo quando nao da.
bool takeOverStopKey(QString *message);

// Devolve a tecla para o "Parar gravacao" do OBS.
bool giveBackStopKey(QString *message);

} // namespace SuiteOutroHotkey
