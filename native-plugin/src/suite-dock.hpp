#pragma once

#include "suite-engine.hpp"
#include "suite-store.hpp"

#include <QList>
#include <QWidget>

class QCheckBox;
class QEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class SuiteDock : public QWidget {
  Q_OBJECT

public:
  SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent = nullptr);

protected:
  // Refaz o calculo de altura dos itens quando o painel muda de largura,
  // para o texto quebrado nao ficar cortado.
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void refresh();
  void onSelectionChanged();
  void onAddSuite();
  void onRenameSuite();
  void onDuplicateSuite();
  void onRemoveSuite();
  void onActivateSelected();
  void onClearActive();
  void onMergeSuites();
  void onSuiteContextMenu(const QPoint &pos);

  void onAddStep();
  void onEditStep();
  void onRemoveStep();
  void onMoveStepUp();
  void onMoveStepDown();
  void onMergeSteps();
  void onUnmergeSteps();
  void onStepContextMenu(const QPoint &pos);
  void onOpenFolderToggled(bool checked);
  void onRunNow();

private:
  QString selectedSuiteId() const;
  QStringList selectedSuiteIds() const;
  Suite *selectedSuite();
  void persistSelectedSuite();
  void selectSuiteById(const QString &id);
  void updateButtonStates();
  bool editStepDialog(SuiteStep &step, bool isNew);

  // Linha atual da lista de passos: indice do passo, ou -1 num cabecalho
  // de grupo.
  int currentStepIndex() const;
  // Grupo da linha atual (vazio quando o passo nao faz parte de um grupo).
  QString currentRowGroupId() const;
  // Indices dos passos marcados, em ordem crescente.
  QList<int> markedStepIndices() const;
  void selectStepRowByIndex(int stepIndex);

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
  QPushButton *m_mergeStepsBtn = nullptr;
  QPushButton *m_unmergeStepsBtn = nullptr;
  QPushButton *m_runBtn = nullptr;

  bool m_updating = false;
};
