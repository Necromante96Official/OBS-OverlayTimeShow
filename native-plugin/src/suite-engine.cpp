#include "suite-engine.hpp"
#include "suite-actions.hpp"
#include "suite-fader.hpp"

#include <obs-module.h>

SuiteEngine::SuiteEngine(SuiteStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &SuiteEngine::advance);
  m_stopRecordingTimer.setSingleShot(true);
  connect(&m_stopRecordingTimer, &QTimer::timeout, this, [this]() {
    emit outroState(false);
    SuiteActions::stopRecording();
  });
}

void SuiteEngine::runActiveOnStart()
{
  // Guarda quando a gravacao comecou: se a mesma tecla inicia e encerra, o
  // encerramento suave nao pode disparar junto com o inicio.
  m_sinceStart.start();

  if (!m_store)
    return;
  const Suite *suite = m_store->activeSuite();
  if (!suite) {
    blog(LOG_INFO, "[obs-overlay-time-show] nenhuma suite ativa no momento");
    return;
  }
  if (!suite->runOnRecordingStart) {
    blog(LOG_INFO,
         "[obs-overlay-time-show] suite ativa nao roda ao iniciar a gravacao");
    return;
  }
  runSuite(*suite, SuiteTrigger::RecordingStarted);
}

void SuiteEngine::runActiveNow(SuiteTrigger trigger)
{
  if (!m_store)
    return;
  const Suite *suite = m_store->activeSuite();
  if (!suite) {
    blog(LOG_INFO, "[obs-overlay-time-show] nenhuma suite ativa no momento");
    return;
  }
  runSuite(*suite, trigger);
}

void SuiteEngine::runSuiteById(const QString &id, SuiteTrigger trigger)
{
  if (!m_store || id.isEmpty())
    return;
  const Suite *suite = m_store->suiteById(id);
  if (!suite) {
    blog(LOG_WARNING, "[obs-overlay-time-show] suite do atalho nao existe mais");
    return;
  }
  runSuite(*suite, trigger);
}

void SuiteEngine::stopRecordingWithOutro()
{
  if (!SuiteActions::isRecording()) {
    blog(LOG_INFO, "[obs-overlay-time-show] nao ha gravacao para encerrar");
    return;
  }
  if (m_sinceStart.isValid() && m_sinceStart.elapsed() < 1200) {
    blog(LOG_INFO,
         "[obs-overlay-time-show] gravacao acabou de comecar, ignorando o "
         "pedido de encerramento");
    return;
  }

  if (m_running && m_stopRecordingWhenDone) {
    // Segunda vez na mesma tecla: encerra na hora, sem esperar a transicao.
    blog(LOG_INFO,
         "[obs-overlay-time-show] encerramento pedido de novo, parando agora");
    cancel();
    m_outroRan = true;
    emit outroState(false);
    SuiteActions::stopRecording();
    return;
  }

  const Suite *suite = m_store ? m_store->activeSuite() : nullptr;
  const bool hasOutro = suite && suite->runOnRecordingStop &&
                        !suite->steps.isEmpty();
  if (!hasOutro) {
    SuiteActions::stopRecording();
    return;
  }

  // O evento de parada nao deve repetir o que estamos rodando agora.
  m_outroRan = true;
  emit outroState(true);
  runSuite(*suite, SuiteTrigger::RecordingStopped, false, true);
}

void SuiteEngine::onRecordingStopped()
{
  if (m_outroRan) {
    // O grupo de encerramento ja rodou antes de a gravacao parar.
    m_outroRan = false;
    emit outroState(false);
    const Suite *suite = m_store ? m_store->activeSuite() : nullptr;
    if (suite && suite->openRecordingFolderOnStop &&
        !SuiteActions::openRecordingFolder()) {
      blog(LOG_WARNING,
           "[obs-overlay-time-show] nao foi possivel abrir pasta da gravacao");
    }
    return;
  }

  cancel();
  if (!m_store)
    return;
  const Suite *suite = m_store->activeSuite();
  if (!suite)
    return;

  if (suite->runOnRecordingStop) {
    // A pasta da gravacao so abre depois que os passos do fim terminarem,
    // para o Explorer nao roubar o foco no meio da automacao.
    runSuite(*suite, SuiteTrigger::RecordingStopped,
             suite->openRecordingFolderOnStop);
    return;
  }

  if (suite->openRecordingFolderOnStop && !SuiteActions::openRecordingFolder()) {
    blog(LOG_WARNING,
         "[obs-overlay-time-show] nao foi possivel abrir pasta da gravacao");
  }
}

void SuiteEngine::cancel()
{
  const bool wasRunning = m_running;
  const bool wasOutroPending =
      m_stopRecordingTimer.isActive() || m_stopRecordingWhenDone;

  // Aplica o volume final dos fades pendentes, para nao deixar audio
  // parado no meio do caminho.
  SuiteFader::finishAllNow();
  m_timer.stop();
  // Cancela um stopRecording deferred (outro) para nao parar gravacao
  // depois de uma suite cancelada ou substituida.
  if (m_stopRecordingTimer.isActive()) {
    m_stopRecordingTimer.stop();
    m_outroRan = false;
  }
  m_queue.clear();
  m_index = 0;
  m_skipNext = false;
  m_skippedGroups.clear();
  m_currentGroupId.clear();
  m_slotGroupId.clear();
  m_openFolderWhenDone = false;
  m_stopRecordingWhenDone = false;
  m_tailMs = 0;
  m_running = false;
  m_suiteName.clear();

  if (wasOutroPending)
    emit outroState(false);
  if (wasRunning)
    emit finished();
}

QString SuiteEngine::slotGroupIdForTrigger(const Suite &suite,
                                           SuiteTrigger trigger)
{
  switch (trigger) {
  case SuiteTrigger::RecordingStarted:
    return suite.groupOnStartId;
  case SuiteTrigger::RecordingStopped:
    return suite.groupOnStopId;
  case SuiteTrigger::Manual:
    return suite.groupOnHotkeyId;
  }
  return QString();
}

bool SuiteEngine::groupAllowed(const SuiteStep &step) const
{
  if (step.groupId.isEmpty())
    return true;
  if (m_skippedGroups.contains(step.groupId))
    return false;
  if (!m_slotGroupId.isEmpty())
    return step.groupId == m_slotGroupId;
  return suiteGroupWhenMatches(step.groupWhen, m_trigger);
}

QVector<SuiteStep> SuiteEngine::buildQueue(const Suite &suite)
{
  QVector<SuiteStep> queue;
  queue.reserve(suite.steps.size() + 4);

  int i = 0;
  while (i < suite.steps.size()) {
    const SuiteStep &first = suite.steps.at(i);
    if (first.groupId.isEmpty()) {
      queue.append(first);
      ++i;
      continue;
    }

    int end = i + 1;
    while (end < suite.steps.size() &&
           suite.steps.at(end).groupId == first.groupId) {
      ++end;
    }

    // O escurecer e o clarear do grupo entram como passos de verdade: um
    // antes das acoes, o outro depois delas.
    SuiteStep fade;
    fade.type = SuiteStepType::ScreenFade;
    fade.screenFadeMs = qMax(1, first.groupFadeMs);
    fade.groupId = first.groupId;
    fade.groupName = first.groupName;
    fade.groupWhen = first.groupWhen;

    if (first.groupFade == SuiteGroupFade::ToBlack) {
      fade.fadeIn = false;
      queue.append(fade);
    }
    for (int k = i; k < end; ++k)
      queue.append(suite.steps.at(k));
    if (first.groupFade == SuiteGroupFade::FromBlack) {
      fade.fadeIn = true;
      queue.append(fade);
    }

    i = end;
  }

  return queue;
}

void SuiteEngine::runSuite(const Suite &suite, SuiteTrigger trigger,
                           bool openFolderWhenDone, bool stopRecordingWhenDone)
{
  cancel();
  m_queue = buildQueue(suite);
  m_index = 0;
  m_trigger = trigger;
  m_slotGroupId = slotGroupIdForTrigger(suite, trigger);
  m_openFolderWhenDone = openFolderWhenDone;
  m_stopRecordingWhenDone = stopRecordingWhenDone;
  m_tailMs = 0;
  m_running = true;
  m_suiteName = suite.name;
  blog(LOG_INFO, "[obs-overlay-time-show] executando suite: %s (%d passos)",
       suite.name.toUtf8().constData(), m_queue.size());
  emit started(suite.name);
  advance();
}

void SuiteEngine::advance()
{
  if (!m_running)
    return;

  while (m_index < m_queue.size()) {
    if (m_skipNext) {
      m_skipNext = false;
      // Se o que vem depois da condicao e um grupo mesclado, pula o grupo
      // inteiro, e nao apenas o primeiro passo dele.
      const QString groupId = m_queue.at(m_index).groupId;
      if (groupId.isEmpty()) {
        ++m_index;
      } else {
        while (m_index < m_queue.size() &&
               m_queue.at(m_index).groupId == groupId) {
          ++m_index;
        }
      }
      continue;
    }

    // Um grupo so roda no momento escolhido nele (ou no slot da suite), e
    // nao roda se alguma condicao o desligou nesta execucao.
    const SuiteStep &pending = m_queue.at(m_index);
    if (!groupAllowed(pending)) {
      ++m_index;
      continue;
    }

    const SuiteStep step = m_queue.at(m_index);
    const int stepIndex = m_index;
    ++m_index;

    if (step.groupId != m_currentGroupId)
      m_currentGroupId = step.groupId;

    emit stepStarted(stepIndex, step.summary());

    if (step.type == SuiteStepType::DelayMs) {
      const int waitMs = qMax(0, step.ms);
      if (waitMs == 0)
        continue;
      m_timer.start(waitMs);
      return;
    }

    if (step.type == SuiteStepType::IfTrigger) {
      applyConditionResult(step, step.trigger == m_trigger);
      continue;
    }

    bool conditionOk = true;
    int waitMs = 0;
    bool failed = false;
    const bool ok = SuiteActions::executeStep(step, &failed, &waitMs);
    if (!ok) {
      blog(LOG_WARNING, "[obs-overlay-time-show] passo falhou: %s",
           step.summary().toUtf8().constData());
      emit stepFailed(step.summary());
    }
    if (step.isCondition()) {
      conditionOk = !failed;
      applyConditionResult(step, conditionOk);
    }

    // Se o passo ja esperou a transicao (waitMs), nao soma de novo no fim.
    // So guarda sobra para o encerramento quando o passo nao bloqueou.
    if (step.type == SuiteStepType::SetSourceVisible && waitMs == 0 &&
        !step.itemTransitionId.isEmpty() &&
        step.itemTransitionId != QLatin1String("none")) {
      m_tailMs = qMax(m_tailMs, step.itemTransitionMs);
    }
    if (waitMs == 0 && step.fadeAudio)
      m_tailMs = qMax(m_tailMs, step.fadeMs);

    // Passos com transicao de audio/fonte podem pedir para esperar terminar.
    if (waitMs > 0) {
      m_timer.start(waitMs);
      return;
    }
  }

  finish();
}

void SuiteEngine::applyConditionResult(const SuiteStep &step, bool passed)
{
  if (passed)
    return;
  if (step.conditionGroupId.isEmpty()) {
    m_skipNext = true;
    return;
  }
  // A condicao aponta para um grupo: ele fica desligado nesta execucao,
  // esteja onde estiver da condicao para baixo.
  m_skippedGroups.insert(step.conditionGroupId);
}

void SuiteEngine::finish()
{
  m_running = false;
  m_queue.clear();
  m_index = 0;
  m_skipNext = false;
  m_skippedGroups.clear();
  m_currentGroupId.clear();
  m_slotGroupId.clear();
  blog(LOG_INFO, "[obs-overlay-time-show] suite finalizada");
  emit finished();

  if (m_stopRecordingWhenDone) {
    m_stopRecordingWhenDone = false;
    // Folga extra apos a ultima transicao: o encoder ainda precisa gravar
    // os ultimos quadros do esmaecer antes de fechar o arquivo.
    const int waitMs = qMax(0, m_tailMs) + 750;
    m_tailMs = 0;
    blog(LOG_INFO,
         "[obs-overlay-time-show] encerrando a gravacao em %d ms", waitMs);
    emit outroState(true);
    m_stopRecordingTimer.start(waitMs);
    return;
  }

  if (m_openFolderWhenDone) {
    m_openFolderWhenDone = false;
    if (!SuiteActions::openRecordingFolder()) {
      blog(LOG_WARNING,
           "[obs-overlay-time-show] nao foi possivel abrir pasta da gravacao");
    }
  }
}
