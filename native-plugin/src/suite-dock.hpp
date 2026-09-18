#pragma once

#include "suite-engine.hpp"
#include "suite-store.hpp"

#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QShowEvent;
class QTabBar;
class QTabWidget;

class SuiteDock : public QWidget {
  Q_OBJECT

public:
  SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent = nullptr);

protected:
  // Refaz o calculo de altura dos itens quando o painel muda de largura,
  // para o texto quebrado nao ficar cortado.
  bool eventFilter(QObject *watched, QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void refresh();
  void onSelectionChanged();
  void onAddSuite();
  void onAddExampleSuite();
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
  void onRunOnStartToggled(bool checked);
  void onRunOnStopToggled(bool checked);
  void onGroupOnStartChanged(int index);
  void onGroupOnStopChanged(int index);
  void onGroupOnHotkeyChanged(int index);
  void onRunNow();
  void onStopWithOutro();
  void onOutroHotkeySetup();

  void onEngineStarted(const QString &suiteName);
  void onEngineStepStarted(int stepIndex, const QString &summary);
  void onEngineStepFailed(const QString &summary);
  void onEngineFinished();
  void onEngineOutroState(bool active);

private:
  QString selectedSuiteId() const;
  QStringList selectedSuiteIds() const;
  Suite *selectedSuite();
  void persistSelectedSuite();
  void selectSuiteById(const QString &id);
  void updateButtonStates();
  bool editStepDialog(SuiteStep &step, bool isNew);
  // Nome do grupo e momento em que ele roda.
  bool groupOptionsDialog(SuiteGroupInfo &info, bool isNew);
  void editGroup(const QString &groupId);
  void setGroupWhen(const QString &groupId, SuiteGroupWhen when);
  // Garante que a suite seja chamada no momento que o grupo espera.
  void ensureSuiteRunsFor(Suite *suite, SuiteGroupWhen when);

  // Linha atual da lista de passos: indice do passo, ou -1 num cabecalho
  // de grupo.
  int currentStepIndex() const;
  // Grupo da linha atual (vazio quando o passo nao faz parte de um grupo).
  QString currentRowGroupId() const;
  // Indices dos passos marcados, em ordem crescente.
  QList<int> markedStepIndices() const;
  void selectStepRowByIndex(int stepIndex);
  // Solta as condicoes que apontavam para um grupo que deixou de existir.
  void releaseConditionTargets(Suite *suite, const QString &groupId);
  void clearSuiteGroupSlots(Suite *suite, const QString &groupId);
  void refreshGroupSlotCombos();
  void applyGroupSlot(QComboBox *combo, QString *slotField,
                      SuiteGroupWhen syncWhen, bool enableRunFlag);
  void highlightRunningStep(int stepIndex);
  void setStatusText(const QString &text);

  SuiteStore *m_store = nullptr;
  SuiteEngine *m_engine = nullptr;

  // Cada aba e uma suite; as abas de conteudo abaixo mostram os passos e as
  // opcoes da suite, uma coisa por vez.
  QTabBar *m_suiteTabs = nullptr;
  QTabWidget *m_pages = nullptr;
  QLabel *m_stepsHeader = nullptr;
  QLabel *m_statusLabel = nullptr;
  QListWidget *m_stepList = nullptr;
  QCheckBox *m_openFolderCheck = nullptr;
  QCheckBox *m_runOnStartCheck = nullptr;
  QCheckBox *m_runOnStopCheck = nullptr;
  QComboBox *m_groupOnStartCombo = nullptr;
  QComboBox *m_groupOnStopCombo = nullptr;
  QComboBox *m_groupOnHotkeyCombo = nullptr;
  QLabel *m_activeLabel = nullptr;
  QWidget *m_emptyState = nullptr;
  QPushButton *m_exampleSuiteBtn = nullptr;

  QPushButton *m_renameBtn = nullptr;
  QPushButton *m_duplicateBtn = nullptr;
  QPushButton *m_removeBtn = nullptr;
  QPushButton *m_mergeBtn = nullptr;
  QPushButton *m_activateBtn = nullptr;
  QPushButton *m_deactivateBtn = nullptr;

  QPushButton *m_addStepBtn = nullptr;
  QPushButton *m_upBtn = nullptr;
  QPushButton *m_downBtn = nullptr;
  QPushButton *m_runBtn = nullptr;
  QPushButton *m_stopWithOutroBtn = nullptr;
  QPushButton *m_outroKeyBtn = nullptr;

  int m_runningStepIndex = -1;
  bool m_updating = false;
};
