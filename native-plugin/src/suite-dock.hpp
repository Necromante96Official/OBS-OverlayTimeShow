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
  void onMergeSelected();
  void onSuiteContextMenu(const QPoint &pos);
  void onOpenFolderToggled(bool checked);
  void onAddStep();
  void onEditStep();
  void onRemoveStep();
  void onMoveStepUp();
  void onMoveStepDown();
  void onRunNow();

private:
  QString selectedSuiteId() const;
  // Ids das suites marcadas, na ordem em que aparecem na lista.
  QStringList selectedSuiteIds() const;
  Suite *selectedSuite();
  void persistSelectedSuite();
  void selectSuiteById(const QString &id);
  void updateButtonStates();
  bool editStepDialog(SuiteStep &step, bool isNew);

  SuiteStore *m_store = nullptr;
  SuiteEngine *m_engine = nullptr;

  QListWidget *m_suiteList = nullptr;
  QListWidget *m_stepList = nullptr;
  QCheckBox *m_openFolderCheck = nullptr;
  QLabel *m_activeLabel = nullptr;

  QPushButton *m_renameBtn = nullptr;
  QPushButton *m_duplicateBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;
  QPushButton *m_mergeBtn = nullptr;
  QPushButton *m_activateBtn = nullptr;
  QPushButton *m_deactivateBtn = nullptr;
  QPushButton *m_addStepBtn = nullptr;
  QPushButton *m_editStepBtn = nullptr;
  QPushButton *m_removeStepBtn = nullptr;
  QPushButton *m_upBtn = nullptr;
  QPushButton *m_downBtn = nullptr;
  QPushButton *m_runBtn = nullptr;

  bool m_updating = false;
};
