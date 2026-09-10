#pragma once

#include "suite-store.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QTimer>

class SuiteEngine : public QObject {
  Q_OBJECT

public:
  explicit SuiteEngine(SuiteStore *store, QObject *parent = nullptr);

  void runActiveOnStart();
  // Execucao manual. O gatilho pode ser simulado para testar os grupos que
  // rodam so no inicio ou so no fim da gravacao.
  void runActiveNow(SuiteTrigger trigger = SuiteTrigger::Manual);
  // Roda uma suite especifica, mesmo que ela nao seja a ativa (atalhos).
  void runSuiteById(const QString &id,
                    SuiteTrigger trigger = SuiteTrigger::Manual);
  void onRecordingStopped();
  // Roda o grupo de encerramento, espera as transicoes e so depois encerra a
  // gravacao, para o esmaecer entrar no arquivo.
  void stopRecordingWithOutro();
  void cancel();
  bool isRunning() const { return m_running; }

private:
  void runSuite(const Suite &suite, SuiteTrigger trigger,
                bool openFolderWhenDone = false,
                bool stopRecordingWhenDone = false);
  // Monta a fila incluindo o escurecer e o clarear pedidos pelos grupos.
  static QVector<SuiteStep> buildQueue(const Suite &suite);
  void advance();
  // Desliga o proximo bloco ou o grupo alvo quando a condicao nao passa.
  void applyConditionResult(const SuiteStep &step, bool passed);
  void finish();

  SuiteStore *m_store = nullptr;
  QTimer m_timer;
  // Stop deferred apos outro — cancelavel via cancel().
  QTimer m_stopRecordingTimer;
  QVector<SuiteStep> m_queue;
  int m_index = 0;
  bool m_running = false;
  bool m_skipNext = false;
  SuiteTrigger m_trigger = SuiteTrigger::Manual;
  // Grupos desligados por uma condicao nesta execucao.
  QSet<QString> m_skippedGroups;
  // Grupo do passo anterior, para aplicar a transicao ao entrar em um novo.
  QString m_currentGroupId;
  bool m_openFolderWhenDone = false;
  bool m_stopRecordingWhenDone = false;
  // Encerramento manual em andamento: o evento de parada nao repete o grupo.
  bool m_outroRan = false;
  // Maior transicao ou fade iniciado e nao esperado, para dar tempo de tudo
  // aparecer no arquivo antes de encerrar.
  int m_tailMs = 0;
  QElapsedTimer m_sinceStart;
};
