#pragma once

#include "camera-position.hpp"
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
class QSpinBox;

class SuiteDock : public QWidget {
  Q_OBJECT

public:
  SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent = nullptr);

protected:
  // Refaz o calculo de altura dos itens quando o painel muda de largura,
  // para o texto quebrado nao ficar cortado.
  bool eventFilter(QObject *watched, QEvent *event) override;
  // As fontes da cena so existem depois que o OBS carrega, por isso a lista
  // de camera e refeita a cada vez que o painel aparece.
  void showEvent(QShowEvent *event) override;

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
  void onRunOnStartToggled(bool checked);
  void onRunOnStopToggled(bool checked);
  void onRunNow();
  void onStopWithOutro();
  void onOutroHotkeySetup();
  void onCameraSourceChanged(int index);
  void onCameraMarginChanged(int value);
  void onCameraAllScenesToggled(bool enabled);
  void onCameraAnchor(CameraPosition::Anchor anchor);
  void onCameraCycle(int direction);

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
  // Relista as fontes de video e recarrega as opcoes de posicao da camera.
  void refreshCameraOptions();
  // Explica por que a camera nao se moveu.
  void warnCameraSourceMissing();

  SuiteStore *m_store = nullptr;
  SuiteEngine *m_engine = nullptr;

  QListWidget *m_suiteList = nullptr;
  QListWidget *m_stepList = nullptr;
  QCheckBox *m_openFolderCheck = nullptr;
  QCheckBox *m_runOnStartCheck = nullptr;
  QCheckBox *m_runOnStopCheck = nullptr;
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
  QPushButton *m_stopWithOutroBtn = nullptr;
  QPushButton *m_outroKeyBtn = nullptr;
  QComboBox *m_cameraSourceCombo = nullptr;
  QSpinBox *m_cameraMarginSpin = nullptr;
  QCheckBox *m_cameraAllScenesCheck = nullptr;

  bool m_updating = false;
};
