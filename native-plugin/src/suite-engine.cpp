#include "suite-engine.hpp"
#include "suite-actions.hpp"
#include "suite-fader.hpp"

#include <obs-module.h>

SuiteEngine::SuiteEngine(SuiteStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &SuiteEngine::advance);
}

void SuiteEngine::runActiveOnStart()
{
  runActiveNow();
}

void SuiteEngine::runActiveNow()
{
  if (!m_store)
    return;
  const Suite *suite = m_store->activeSuite();
  if (!suite) {
    blog(LOG_INFO, "[obs-overlay-time-show] nenhuma suite ativa no momento");
    return;
  }
  runSuite(*suite);
}

void SuiteEngine::onRecordingStopped()
{
  cancel();
  if (!m_store)
    return;
  const Suite *suite = m_store->activeSuite();
  if (!suite || !suite->openRecordingFolderOnStop)
    return;
  if (!SuiteActions::openRecordingFolder()) {
    blog(LOG_WARNING,
         "[obs-overlay-time-show] nao foi possivel abrir pasta da gravacao");
  }
}

void SuiteEngine::cancel()
{
  // Aplica o volume final dos fades pendentes, para nao deixar audio
  // parado no meio do caminho.
  SuiteFader::finishAllNow();
  m_timer.stop();
  m_queue.clear();
  m_index = 0;
  m_skipNext = false;
  m_running = false;
}

void SuiteEngine::runSuite(const Suite &suite)
{
  cancel();
  m_queue = suite.steps;
  m_index = 0;
  m_running = true;
  blog(LOG_INFO, "[obs-overlay-time-show] executando suite: %s (%d passos)",
       suite.name.toUtf8().constData(), m_queue.size());
  advance();
}

void SuiteEngine::advance()
{
  if (!m_running)
    return;

  while (m_index < m_queue.size()) {
    if (m_skipNext) {
      m_skipNext = false;
      ++m_index;
      continue;
    }

    const SuiteStep step = m_queue.at(m_index);
    ++m_index;

    if (step.type == SuiteStepType::DelayMs) {
      const int waitMs = qMax(0, step.ms);
      if (waitMs == 0)
        continue;
      m_timer.start(waitMs);
      return;
    }

    bool skipNext = false;
    int waitMs = 0;
    const bool ok = SuiteActions::executeStep(step, &skipNext, &waitMs);
    if (!ok) {
      blog(LOG_WARNING, "[obs-overlay-time-show] passo falhou: %s",
           step.summary().toUtf8().constData());
    }
    m_skipNext = skipNext;

    // Passos com transicao de audio podem pedir para esperar o fade terminar.
    if (waitMs > 0) {
      m_timer.start(waitMs);
      return;
    }
  }

  finish();
}

void SuiteEngine::finish()
{
  m_running = false;
  m_queue.clear();
  m_index = 0;
  m_skipNext = false;
  blog(LOG_INFO, "[obs-overlay-time-show] suite finalizada");
}
