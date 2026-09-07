#pragma once

#include "suite-engine.hpp"
#include "suite-store.hpp"

#include <QWidget>

class QListWidget;
class QCheckBox;
class QLabel;
class QPushButton;

class SuiteDock : public QWidget {
  Q_OBJECT

public:
  SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent = nullptr);

private slots:
  void refresh();
  void onSelectionChanged();
  void onAddSuite();
  void onRenameSuite();
  void onDuplicateSuite();
  void onRemoveSuite();
  void onActivateSelected();
  void onClearActive();
  void onOpenFolderToggled(bool checked);
  void onAddStep();
  void onEditStep();
  void onRemoveStep();
  void onMoveStepUp();
  void onMoveStepDown();
  void onRunNow();

private:
  QString selectedSuiteId() const;
  Suite *selectedSuite();
  void persistSelectedSuite();
  bool editStepDialog(SuiteStep &step, bool isNew);

  SuiteStore *m_store = nullptr;
  SuiteEngine *m_engine = nullptr;

  QListWidget *m_suiteList = nullptr;
  QListWidget *m_stepList = nullptr;
  QCheckBox *m_openFolderCheck = nullptr;
  QLabel *m_activeLabel = nullptr;
  bool m_updating = false;
};
