#include "suite-dock.hpp"
#include "suite-actions.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QAbstractItemView>

namespace {

QString stepTypeLabel(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
    return QObject::tr("Trocar cena");
  case SuiteStepType::SetSourceVisible:
    return QObject::tr("Mostrar/ocultar fonte");
  case SuiteStepType::DelayMs:
    return QObject::tr("Esperar");
  case SuiteStepType::SetTransition:
    return QObject::tr("Definir transicao");
  case SuiteStepType::SetMute:
    return QObject::tr("Mute/unmute");
  case SuiteStepType::SetVolume:
    return QObject::tr("Volume");
  case SuiteStepType::IfCurrentScene:
    return QObject::tr("Se cena atual");
  case SuiteStepType::IfSourceVisible:
    return QObject::tr("Se fonte visivel");
  case SuiteStepType::RestartMedia:
    return QObject::tr("Reiniciar midia");
  case SuiteStepType::OpenUrl:
    return QObject::tr("Abrir URL/programa");
  }
  return QObject::tr("Passo");
}

} // namespace

SuiteDock::SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent)
    : QWidget(parent), m_store(store), m_engine(engine)
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  m_activeLabel = new QLabel(this);
  root->addWidget(m_activeLabel);

  auto *suiteRow = new QHBoxLayout();
  m_suiteList = new QListWidget(this);
  m_suiteList->setSelectionMode(QAbstractItemView::SingleSelection);
  suiteRow->addWidget(m_suiteList, 1);

  auto *suiteButtons = new QVBoxLayout();
  auto *addBtn = new QPushButton(tr("Nova"), this);
  auto *renameBtn = new QPushButton(tr("Renomear"), this);
  auto *dupBtn = new QPushButton(tr("Duplicar"), this);
  auto *removeBtn = new QPushButton(tr("Excluir"), this);
  auto *activateBtn = new QPushButton(tr("Ativar"), this);
  auto *clearActiveBtn = new QPushButton(tr("Desativar"), this);
  suiteButtons->addWidget(addBtn);
  suiteButtons->addWidget(renameBtn);
  suiteButtons->addWidget(dupBtn);
  suiteButtons->addWidget(removeBtn);
  suiteButtons->addSpacing(8);
  suiteButtons->addWidget(activateBtn);
  suiteButtons->addWidget(clearActiveBtn);
  suiteButtons->addStretch(1);
  suiteRow->addLayout(suiteButtons);
  root->addLayout(suiteRow, 1);

  m_openFolderCheck =
      new QCheckBox(tr("Ao terminar, abrir pasta da gravacao"), this);
  root->addWidget(m_openFolderCheck);

  root->addWidget(new QLabel(tr("Passos (ao iniciar gravacao / atalho)"), this));
  m_stepList = new QListWidget(this);
  root->addWidget(m_stepList, 2);

  auto *stepButtons = new QHBoxLayout();
  auto *addStepBtn = new QPushButton(tr("Adicionar"), this);
  auto *editStepBtn = new QPushButton(tr("Editar"), this);
  auto *removeStepBtn = new QPushButton(tr("Remover"), this);
  auto *upBtn = new QPushButton(tr("Subir"), this);
  auto *downBtn = new QPushButton(tr("Descer"), this);
  auto *runBtn = new QPushButton(tr("Executar agora"), this);
  stepButtons->addWidget(addStepBtn);
  stepButtons->addWidget(editStepBtn);
  stepButtons->addWidget(removeStepBtn);
  stepButtons->addWidget(upBtn);
  stepButtons->addWidget(downBtn);
  stepButtons->addStretch(1);
  stepButtons->addWidget(runBtn);
  root->addLayout(stepButtons);

  connect(m_store, &SuiteStore::changed, this, &SuiteDock::refresh);
  connect(m_suiteList, &QListWidget::itemSelectionChanged, this,
          &SuiteDock::onSelectionChanged);
  connect(addBtn, &QPushButton::clicked, this, &SuiteDock::onAddSuite);
  connect(renameBtn, &QPushButton::clicked, this, &SuiteDock::onRenameSuite);
  connect(dupBtn, &QPushButton::clicked, this, &SuiteDock::onDuplicateSuite);
  connect(removeBtn, &QPushButton::clicked, this, &SuiteDock::onRemoveSuite);
  connect(activateBtn, &QPushButton::clicked, this, &SuiteDock::onActivateSelected);
  connect(clearActiveBtn, &QPushButton::clicked, this, &SuiteDock::onClearActive);
  connect(m_openFolderCheck, &QCheckBox::toggled, this,
          &SuiteDock::onOpenFolderToggled);
  connect(addStepBtn, &QPushButton::clicked, this, &SuiteDock::onAddStep);
  connect(editStepBtn, &QPushButton::clicked, this, &SuiteDock::onEditStep);
  connect(removeStepBtn, &QPushButton::clicked, this, &SuiteDock::onRemoveStep);
  connect(upBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepUp);
  connect(downBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepDown);
  connect(runBtn, &QPushButton::clicked, this, &SuiteDock::onRunNow);

  refresh();
}

QString SuiteDock::selectedSuiteId() const
{
  auto *item = m_suiteList->currentItem();
  if (!item)
    return {};
  return item->data(Qt::UserRole).toString();
}

Suite *SuiteDock::selectedSuite()
{
  return m_store ? m_store->suiteById(selectedSuiteId()) : nullptr;
}

void SuiteDock::persistSelectedSuite()
{
  Suite *suite = selectedSuite();
  if (!suite || !m_store)
    return;
  m_store->updateSuite(*suite);
}

void SuiteDock::refresh()
{
  if (!m_store)
    return;

  m_updating = true;
  const QString previous = selectedSuiteId();
  m_suiteList->clear();

  for (const Suite &suite : m_store->suites()) {
    const bool active = suite.id == m_store->activeSuiteId();
    auto *item = new QListWidgetItem(
        QStringLiteral("%1%2").arg(active ? QStringLiteral("● ") : QString(),
                                   suite.name),
        m_suiteList);
    item->setData(Qt::UserRole, suite.id);
  }

  int selectRow = 0;
  for (int i = 0; i < m_suiteList->count(); ++i) {
    if (m_suiteList->item(i)->data(Qt::UserRole).toString() == previous) {
      selectRow = i;
      break;
    }
  }
  if (m_suiteList->count() > 0)
    m_suiteList->setCurrentRow(selectRow);

  const Suite *active = m_store->activeSuite();
  m_activeLabel->setText(active ? tr("Suite ativa: %1").arg(active->name)
                                : tr("Nenhuma suite ativa"));

  m_updating = false;
  onSelectionChanged();
}

void SuiteDock::onSelectionChanged()
{
  if (m_updating)
    return;

  Suite *suite = selectedSuite();
  m_stepList->clear();
  m_openFolderCheck->blockSignals(true);
  if (!suite) {
    m_openFolderCheck->setChecked(false);
    m_openFolderCheck->setEnabled(false);
    m_openFolderCheck->blockSignals(false);
    return;
  }

  m_openFolderCheck->setEnabled(true);
  m_openFolderCheck->setChecked(suite->openRecordingFolderOnStop);
  m_openFolderCheck->blockSignals(false);

  for (const SuiteStep &step : suite->steps) {
    m_stepList->addItem(QStringLiteral("[%1] %2")
                            .arg(stepTypeLabel(step.type), step.summary()));
  }
}

void SuiteDock::onAddSuite()
{
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Nova suite"), tr("Nome do jogo / suite:"), QLineEdit::Normal,
      tr("Nova suite"), &ok);
  if (!ok || name.trimmed().isEmpty())
    return;
  const Suite created = m_store->createSuite(name);
  refresh();
  for (int i = 0; i < m_suiteList->count(); ++i) {
    if (m_suiteList->item(i)->data(Qt::UserRole).toString() == created.id) {
      m_suiteList->setCurrentRow(i);
      break;
    }
  }
}

void SuiteDock::onRenameSuite()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  bool ok = false;
  const QString name = QInputDialog::getText(this, tr("Renomear suite"),
                                             tr("Novo nome:"), QLineEdit::Normal,
                                             suite->name, &ok);
  if (!ok || name.trimmed().isEmpty())
    return;
  m_store->renameSuite(suite->id, name);
}

void SuiteDock::onDuplicateSuite()
{
  const QString id = selectedSuiteId();
  if (id.isEmpty())
    return;
  const Suite copy = m_store->duplicateSuite(id);
  if (copy.id.isEmpty())
    return;
  refresh();
  for (int i = 0; i < m_suiteList->count(); ++i) {
    if (m_suiteList->item(i)->data(Qt::UserRole).toString() == copy.id) {
      m_suiteList->setCurrentRow(i);
      break;
    }
  }
}

void SuiteDock::onRemoveSuite()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const auto answer = QMessageBox::question(
      this, tr("Excluir suite"),
      tr("Excluir a suite \"%1\"?").arg(suite->name));
  if (answer != QMessageBox::Yes)
    return;
  m_store->removeSuite(suite->id);
}

void SuiteDock::onActivateSelected()
{
  const QString id = selectedSuiteId();
  if (id.isEmpty())
    return;
  m_store->setActiveSuite(id);
}

void SuiteDock::onClearActive()
{
  m_store->clearActiveSuite();
}

void SuiteDock::onOpenFolderToggled(bool checked)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  suite->openRecordingFolderOnStop = checked;
  persistSelectedSuite();
}

bool SuiteDock::editStepDialog(SuiteStep &step, bool isNew)
{
  QDialog dialog(this);
  dialog.setWindowTitle(isNew ? tr("Adicionar passo") : tr("Editar passo"));
  auto *form = new QFormLayout(&dialog);

  auto *typeCombo = new QComboBox(&dialog);
  const QVector<SuiteStepType> types = {
      SuiteStepType::SetScene,         SuiteStepType::SetSourceVisible,
      SuiteStepType::DelayMs,          SuiteStepType::SetTransition,
      SuiteStepType::SetMute,          SuiteStepType::SetVolume,
      SuiteStepType::IfCurrentScene,   SuiteStepType::IfSourceVisible,
      SuiteStepType::RestartMedia,     SuiteStepType::OpenUrl,
  };
  for (SuiteStepType type : types)
    typeCombo->addItem(stepTypeLabel(type), static_cast<int>(type));

  auto *sceneCombo = new QComboBox(&dialog);
  sceneCombo->setEditable(true);
  sceneCombo->addItems(SuiteActions::sceneNames());

  auto *sourceCombo = new QComboBox(&dialog);
  sourceCombo->setEditable(true);

  auto *transitionCombo = new QComboBox(&dialog);
  transitionCombo->setEditable(true);
  transitionCombo->addItems(SuiteActions::transitionNames());

  auto *visibleCheck = new QCheckBox(tr("Visivel / mostrar"), &dialog);
  auto *mutedCheck = new QCheckBox(tr("Mute"), &dialog);
  auto *delaySpin = new QSpinBox(&dialog);
  delaySpin->setRange(0, 600000);
  delaySpin->setSuffix(QStringLiteral(" ms"));
  auto *transitionSpin = new QSpinBox(&dialog);
  transitionSpin->setRange(0, 60000);
  transitionSpin->setSuffix(QStringLiteral(" ms"));
  auto *volumeSpin = new QDoubleSpinBox(&dialog);
  volumeSpin->setRange(0.0, 100.0);
  volumeSpin->setSuffix(QStringLiteral(" %"));
  volumeSpin->setDecimals(1);
  auto *urlEdit = new QLineEdit(&dialog);

  form->addRow(tr("Tipo"), typeCombo);
  form->addRow(tr("Cena"), sceneCombo);
  form->addRow(tr("Fonte"), sourceCombo);
  form->addRow(tr("Transicao"), transitionCombo);
  form->addRow(tr("Duracao transicao"), transitionSpin);
  form->addRow(tr("Delay"), delaySpin);
  form->addRow(QString(), visibleCheck);
  form->addRow(QString(), mutedCheck);
  form->addRow(tr("Volume"), volumeSpin);
  form->addRow(tr("URL / caminho"), urlEdit);

  auto refreshSources = [&]() {
    const QString current = sourceCombo->currentText();
    sourceCombo->clear();
    QStringList names = SuiteActions::sourceNamesInScene(sceneCombo->currentText());
    if (names.isEmpty())
      names = SuiteActions::audioSourceNames();
    sourceCombo->addItems(names);
    if (!current.isEmpty())
      sourceCombo->setCurrentText(current);
  };

  auto updateVisibility = [&]() {
    const auto type =
        static_cast<SuiteStepType>(typeCombo->currentData().toInt());
    const bool needScene =
        type == SuiteStepType::SetScene ||
        type == SuiteStepType::SetSourceVisible ||
        type == SuiteStepType::IfCurrentScene ||
        type == SuiteStepType::IfSourceVisible;
    const bool needSource =
        type == SuiteStepType::SetSourceVisible ||
        type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::IfSourceVisible ||
        type == SuiteStepType::RestartMedia;
    const bool needTransition =
        type == SuiteStepType::SetScene || type == SuiteStepType::SetTransition;
    sceneCombo->setEnabled(needScene || type == SuiteStepType::SetSourceVisible);
    sourceCombo->setEnabled(needSource);
    transitionCombo->setEnabled(needTransition);
    transitionSpin->setEnabled(needTransition);
    delaySpin->setEnabled(type == SuiteStepType::DelayMs);
    visibleCheck->setEnabled(type == SuiteStepType::SetSourceVisible ||
                             type == SuiteStepType::IfSourceVisible);
    mutedCheck->setEnabled(type == SuiteStepType::SetMute);
    volumeSpin->setEnabled(type == SuiteStepType::SetVolume);
    urlEdit->setEnabled(type == SuiteStepType::OpenUrl);
    if (type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::RestartMedia) {
      sourceCombo->clear();
      sourceCombo->addItems(SuiteActions::audioSourceNames());
      QStringList media = SuiteActions::sourceNamesInScene(sceneCombo->currentText());
      for (const QString &name : media) {
        if (sourceCombo->findText(name) < 0)
          sourceCombo->addItem(name);
      }
    } else {
      refreshSources();
    }
  };

  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, updateVisibility);
  connect(sceneCombo, &QComboBox::currentTextChanged, &dialog,
          [&](const QString &) { refreshSources(); });

  // Prefill
  for (int i = 0; i < typeCombo->count(); ++i) {
    if (static_cast<SuiteStepType>(typeCombo->itemData(i).toInt()) == step.type) {
      typeCombo->setCurrentIndex(i);
      break;
    }
  }
  sceneCombo->setCurrentText(step.scene);
  refreshSources();
  sourceCombo->setCurrentText(step.source);
  transitionCombo->setCurrentText(step.transition);
  transitionSpin->setValue(step.transitionMs);
  delaySpin->setValue(step.ms);
  visibleCheck->setChecked(step.visible);
  mutedCheck->setChecked(step.muted);
  volumeSpin->setValue(step.volume * 100.0);
  urlEdit->setText(step.url);
  updateVisibility();

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return false;

  step.type = static_cast<SuiteStepType>(typeCombo->currentData().toInt());
  step.scene = sceneCombo->currentText().trimmed();
  step.source = sourceCombo->currentText().trimmed();
  step.transition = transitionCombo->currentText().trimmed();
  step.transitionMs = transitionSpin->value();
  step.ms = delaySpin->value();
  step.visible = visibleCheck->isChecked();
  step.muted = mutedCheck->isChecked();
  step.volume = volumeSpin->value() / 100.0;
  step.url = urlEdit->text().trimmed();
  return true;
}

void SuiteDock::onAddStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  SuiteStep step;
  step.type = SuiteStepType::DelayMs;
  step.ms = 1000;
  if (!editStepDialog(step, true))
    return;
  suite->steps.append(step);
  persistSelectedSuite();
}

void SuiteDock::onEditStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const int row = m_stepList->currentRow();
  if (row < 0 || row >= suite->steps.size())
    return;
  SuiteStep step = suite->steps.at(row);
  if (!editStepDialog(step, false))
    return;
  suite->steps[row] = step;
  persistSelectedSuite();
}

void SuiteDock::onRemoveStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const int row = m_stepList->currentRow();
  if (row < 0 || row >= suite->steps.size())
    return;
  suite->steps.removeAt(row);
  persistSelectedSuite();
}

void SuiteDock::onMoveStepUp()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const int row = m_stepList->currentRow();
  if (row <= 0 || row >= suite->steps.size())
    return;
  suite->steps.swapItemsAt(row, row - 1);
  persistSelectedSuite();
  m_stepList->setCurrentRow(row - 1);
}

void SuiteDock::onMoveStepDown()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const int row = m_stepList->currentRow();
  if (row < 0 || row >= suite->steps.size() - 1)
    return;
  suite->steps.swapItemsAt(row, row + 1);
  persistSelectedSuite();
  m_stepList->setCurrentRow(row + 1);
}

void SuiteDock::onRunNow()
{
  if (m_engine)
    m_engine->runActiveNow();
}
