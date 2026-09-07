#pragma once

// Filtro de efeito que arredonda os cantos de qualquer fonte (camera, captura,
// imagem) e tambem sabe recortar um circulo perfeito no centro. O raio e a
// suavizacao da borda sao calculados em pixels reais, entao a forma nao
// distorce quando a fonte nao e quadrada.
namespace RoundedCornersFilter {

void registerFilter();

} // namespace RoundedCornersFilter
