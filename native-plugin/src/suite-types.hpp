#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

enum class SuiteStepType {
  SetScene,
  SetSourceVisible,
  DelayMs,
  SetTransition,
  SetMute,
  SetVolume,
  AudioFade,
  ScreenFade,
  IfCurrentScene,
  IfSourceVisible,
  IfTrigger,
  RestartMedia,
  OpenUrl,
};

// O que fez a suite comecar a rodar.
enum class SuiteTrigger {
  RecordingStarted,
  RecordingStopped,
  Manual,
};

QString suiteTriggerToString(SuiteTrigger trigger);
SuiteTrigger suiteTriggerFromString(const QString &value);
QString suiteTriggerLabel(SuiteTrigger trigger);

// Momento em que um grupo mesclado roda.
enum class SuiteGroupWhen {
  Always,
  RecordingStarted,
  RecordingStopped,
  Manual,
};

// Transicao de tela que o grupo faz: sair do preto no fim das acoes, ou ir
// ate o preto antes delas.
enum class SuiteGroupFade {
  None,
  FromBlack,
  ToBlack,
};

QString suiteGroupFadeToString(SuiteGroupFade fade);
SuiteGroupFade suiteGroupFadeFromString(const QString &value);
QString suiteGroupFadeOption(SuiteGroupFade fade);

QString suiteGroupWhenToString(SuiteGroupWhen when);
SuiteGroupWhen suiteGroupWhenFromString(const QString &value);
// Texto curto para o cabecalho do grupo.
QString suiteGroupWhenLabel(SuiteGroupWhen when);
// Texto da opcao na lista de escolha.
QString suiteGroupWhenOption(SuiteGroupWhen when);
bool suiteGroupWhenMatches(SuiteGroupWhen when, SuiteTrigger trigger);

struct SuiteStep {
  SuiteStepType type = SuiteStepType::DelayMs;

  QString scene;
  QString source;
  QString transition;
  QString url;

  int transitionMs = 300;
  int ms = 1000;
  bool visible = true;
  bool muted = true;
  double volume = 1.0;

  // Transicao de audio (fade). fadeAudio liga o fade nos passos de volume e de
  // mostrar/ocultar fonte; fadeIn escolhe a direcao no passo AudioFade.
  bool fadeAudio = false;
  bool fadeIn = true;
  int fadeMs = 500;
  bool waitForFade = true;

  // Escurecer ou clarear a tela toda (fade de preto sobre a cena).
  int screenFadeMs = 2000;

  // Mesclagem de passos: passos vizinhos com o mesmo groupId formam um grupo.
  // Eles aparecem juntos na lista e uma condicao antes do grupo vale para o
  // grupo inteiro, e nao so para o primeiro passo.
  // Transicao de mostrar/ocultar da fonte (a mesma do OBS, por item da cena).
  // Vazio = nao mexer no que ja esta configurado; "none" = sem transicao.
  QString itemTransitionId;
  QString itemTransitionName;
  int itemTransitionMs = 300;

  QString groupId;
  QString groupName;
  // Todos os passos de um grupo carregam o mesmo momento de execucao e a
  // mesma transicao de tela.
  SuiteGroupWhen groupWhen = SuiteGroupWhen::Always;
  SuiteGroupFade groupFade = SuiteGroupFade::None;
  int groupFadeMs = 2000;

  // Gatilho esperado pelo passo de condicao IfTrigger.
  SuiteTrigger trigger = SuiteTrigger::RecordingStarted;

  // Alvo das condicoes: vazio significa "o proximo passo ou grupo abaixo".
  // Preenchido, a condicao liga ou desliga aquele grupo especifico.
  QString conditionGroupId;
  QString conditionGroupName;

  bool isCondition() const
  {
    return type == SuiteStepType::IfCurrentScene ||
           type == SuiteStepType::IfSourceVisible ||
           type == SuiteStepType::IfTrigger;
  }

  QJsonObject toJson() const;
  static SuiteStep fromJson(const QJsonObject &obj);
  QString summary() const;
  // "o que vem logo abaixo" ou "o grupo X", usado nos textos das condicoes.
  QString conditionTargetText() const;
};

// Opcoes de um grupo mesclado, editadas de uma vez no painel.
struct SuiteGroupInfo {
  QString name;
  SuiteGroupWhen when = SuiteGroupWhen::Always;
  SuiteGroupFade fade = SuiteGroupFade::None;
  int fadeMs = 2000;
};

struct Suite {
  QString id;
  QString name;
  bool openRecordingFolderOnStop = true;
  // Quando a suite roda sozinha. Rodando nos dois momentos, use condicoes de
  // gatilho para separar o que acontece em cada um.
  bool runOnRecordingStart = true;
  bool runOnRecordingStop = false;
  QVector<SuiteStep> steps;

  QJsonObject toJson() const;
  static Suite fromJson(const QJsonObject &obj);
};

QString suiteStepTypeToString(SuiteStepType type);
SuiteStepType suiteStepTypeFromString(const QString &value);
