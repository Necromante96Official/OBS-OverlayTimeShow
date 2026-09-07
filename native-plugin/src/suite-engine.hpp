#pragma once

#include "suite-store.hpp"

#include <QObject>
#include <QTimer>

class SuiteEngine : public QObject {
  Q_OBJECT

public:
  explicit SuiteEngine(SuiteStore *store, QObject *parent = nullptr);

  void runActiveOnStart();
  void runActiveNow();
  // Roda uma suite especifica, mesmo que ela nao seja a ativa (atalhos).
  void runSuiteById(const QString &id);
  void onRecordingStopped();
  void cancel();
  bool isRunning() const { return m_running; }

private:
  void runSuite(const Suite &suite);
  void advance();
  void finish();

  SuiteStore *m_store = nullptr;
  QTimer m_timer;
  QVector<SuiteStep> m_queue;
  int m_index = 0;
  bool m_running = false;
  bool m_skipNext = false;
};
