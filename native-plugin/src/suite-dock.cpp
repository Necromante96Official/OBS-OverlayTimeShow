#include "suite-dock.hpp"
#include "suite-actions.hpp"

#include <obs-module.h>

#include <QAbstractItemView>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QString stepCountText(int count)
{
  return count == 1 ? QObject::tr("1 passo")
                    : QObject::tr("%1 passos").arg(count);
}

QString stepTypeLabel(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
    return QObject::tr("Trocar de cena");
  case SuiteStepType::SetTransition:
    return QObject::tr("Definir a transição padrão");
  case SuiteStepType::SetSourceVisible:
    return QObject::tr("Mostrar ou ocultar uma fonte");
  case SuiteStepType::RestartMedia:
    return QObject::tr("Reiniciar uma mídia");
  case SuiteStepType::SetMute:
    return QObject::tr("Silenciar ou reativar um áudio");
  case SuiteStepType::SetVolume:
    return QObject::tr("Ajustar o volume de um áudio");
  case SuiteStepType::AudioFade:
    return QObject::tr("Transição de áudio (fade in / fade out)");
  case SuiteStepType::DelayMs:
    return QObject::tr("Esperar um tempo");
  case SuiteStepType::IfCurrentScene:
    return QObject::tr("Condição: a cena atual é...");
  case SuiteStepType::IfSourceVisible:
    return QObject::tr("Condição: a fonte está visível ou oculta");
  case SuiteStepType::OpenUrl:
    return QObject::tr("Abrir um programa, pasta ou site");
  }
  return QObject::tr("Passo");
}

QString stepTypeHelp(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
    return QObject::tr(
        "Muda o programa para a cena escolhida. Se você informar uma transição, "
        "ela é aplicada antes da troca.");
  case SuiteStepType::SetTransition:
    return QObject::tr(
        "Define qual transição o OBS vai usar nas próximas trocas de cena, sem "
        "trocar de cena agora.");
  case SuiteStepType::SetSourceVisible:
    return QObject::tr(
        "Liga (visível) ou desliga (oculta) o olho de uma fonte dentro da cena "
        "escolhida. Se a fonte tiver áudio, você pode pedir fade: ao mostrar, o "
        "som sobe do silêncio; ao ocultar, o som desce antes de a fonte "
        "desaparecer.");
  case SuiteStepType::RestartMedia:
    return QObject::tr(
        "Faz um vídeo, áudio ou música voltar ao começo e tocar de novo.");
  case SuiteStepType::SetMute:
    return QObject::tr(
        "Deixa a fonte de áudio no mudo ou tira do mudo no mixer de áudio.");
  case SuiteStepType::SetVolume:
    return QObject::tr(
        "Define o volume da fonte de áudio: 0% é sem som e 100% é o volume "
        "cheio. Você pode fazer a mudança na hora ou deslizando aos poucos "
        "(fade).");
  case SuiteStepType::AudioFade:
    return QObject::tr(
        "Sobe o áudio do silêncio até o volume escolhido (fade in) ou desce "
        "até o silêncio (fade out), sem cortes bruscos. Vale para qualquer "
        "fonte que tenha áudio: microfone, jogo, música, vídeo.");
  case SuiteStepType::DelayMs:
    return QObject::tr(
        "Faz uma pausa antes de executar o próximo passo. Use para dar tempo à "
        "transição ou ao jogo abrir.");
  case SuiteStepType::IfCurrentScene:
    return QObject::tr(
        "Verifica se a cena que está no ar é a escolhida. Se não for, o passo "
        "logo abaixo desta condição é ignorado.");
  case SuiteStepType::IfSourceVisible:
    return QObject::tr(
        "Verifica se a fonte está visível (ou oculta, se você escolher assim). "
        "Se a verificação falhar, o passo logo abaixo desta condição é "
        "ignorado.");
  case SuiteStepType::OpenUrl:
    return QObject::tr(
        "Abre um site, uma pasta do Windows ou um programa. Exemplos: "
        "https://seusite.com ou D:\\Videos.");
  }
  return QString();
}

} // namespace

SuiteDock::SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent)
    : QWidget(parent), m_store(store), m_engine(engine)
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(10);

  m_activeLabel = new QLabel(this);
  QFont activeFont = m_activeLabel->font();
  activeFont.setBold(true);
  m_activeLabel->setFont(activeFont);
  m_activeLabel->setWordWrap(true);
  root->addWidget(m_activeLabel);

  auto *suitesGroup = new QGroupBox(tr("Suítes (uma configuração por jogo)"), this);
  auto *suitesLayout = new QVBoxLayout(suitesGroup);
  suitesLayout->setSpacing(6);

  auto *suitesHint = new QLabel(
      tr("Só uma suíte fica ativa por vez: é ela que roda quando a gravação "
         "começa. Cada suíte também pode ter o seu próprio atalho de teclado, "
         "para rodar quando você quiser."),
      suitesGroup);
  suitesHint->setWordWrap(true);
  suitesHint->setEnabled(false);
  suitesLayout->addWidget(suitesHint);

  auto *mergeHint = new QLabel(
      tr("Para juntar suítes: clique na primeira, segure Shift e clique na "
         "última (ou use Ctrl para escolher uma a uma) e clique com o botão "
         "direito em \"Mesclar\"."),
      suitesGroup);
  mergeHint->setWordWrap(true);
  mergeHint->setEnabled(false);
  suitesLayout->addWidget(mergeHint);

  auto *suiteRow = new QHBoxLayout();
  suiteRow->setSpacing(6);
  m_suiteList = new QListWidget(suitesGroup);
  m_suiteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_suiteList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_suiteList->setToolTip(
      tr("Clique duas vezes para ativar. Shift ou Ctrl marcam várias, e o botão "
         "direito abre as opções."));
  suiteRow->addWidget(m_suiteList, 1);

  auto *suiteButtons = new QVBoxLayout();
  suiteButtons->setSpacing(4);
  auto *addBtn = new QPushButton(tr("Nova suíte"), suitesGroup);
  addBtn->setToolTip(tr("Cria uma suíte vazia para um jogo."));
  m_renameBtn = new QPushButton(tr("Renomear"), suitesGroup);
  m_renameBtn->setToolTip(tr("Muda o nome da suíte selecionada."));
  m_duplicateBtn = new QPushButton(tr("Duplicar"), suitesGroup);
  m_duplicateBtn->setToolTip(
      tr("Cria uma cópia da suíte selecionada, com todos os passos."));
  m_removeBtn = new QPushButton(tr("Excluir"), suitesGroup);
  m_removeBtn->setToolTip(tr("Apaga a suíte selecionada."));
  m_mergeBtn = new QPushButton(tr("Mesclar selecionadas"), suitesGroup);
  m_mergeBtn->setToolTip(
      tr("Junta os passos das suítes marcadas em uma suíte nova, na ordem da "
         "lista."));
  m_activateBtn = new QPushButton(tr("Ativar esta suíte"), suitesGroup);
  m_activateBtn->setToolTip(
      tr("Torna a suíte selecionada a única ativa."));
  m_deactivateBtn = new QPushButton(tr("Desativar todas"), suitesGroup);
  m_deactivateBtn->setToolTip(
      tr("Nenhuma suíte roda ao iniciar a gravação."));
  suiteButtons->addWidget(addBtn);
  suiteButtons->addWidget(m_renameBtn);
  suiteButtons->addWidget(m_duplicateBtn);
  suiteButtons->addWidget(m_removeBtn);
  suiteButtons->addSpacing(10);
  suiteButtons->addWidget(m_mergeBtn);
  suiteButtons->addSpacing(10);
  suiteButtons->addWidget(m_activateBtn);
  suiteButtons->addWidget(m_deactivateBtn);
  suiteButtons->addStretch(1);
  suiteRow->addLayout(suiteButtons);
  suitesLayout->addLayout(suiteRow, 1);
  root->addWidget(suitesGroup, 1);

  auto *optionsGroup = new QGroupBox(tr("Opções da suíte selecionada"), this);
  auto *optionsLayout = new QVBoxLayout(optionsGroup);
  m_openFolderCheck = new QCheckBox(
      tr("Abrir a pasta da gravação quando a gravação terminar"), optionsGroup);
  m_openFolderCheck->setToolTip(
      tr("Ao parar a gravação, o Windows abre a pasta e seleciona o arquivo que "
         "acabou de ser salvo."));
  optionsLayout->addWidget(m_openFolderCheck);
  root->addWidget(optionsGroup);

  auto *stepsGroup = new QGroupBox(tr("Passos da suíte selecionada"), this);
  auto *stepsLayout = new QVBoxLayout(stepsGroup);
  stepsLayout->setSpacing(6);

  auto *stepsHint = new QLabel(
      tr("Os passos rodam de cima para baixo quando a gravação começa ou quando "
         "você usa o atalho de teclado."),
      stepsGroup);
  stepsHint->setWordWrap(true);
  stepsHint->setEnabled(false);
  stepsLayout->addWidget(stepsHint);

  m_stepList = new QListWidget(stepsGroup);
  m_stepList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_stepList->setAlternatingRowColors(true);
  m_stepList->setToolTip(tr("Clique duas vezes em um passo para editá-lo."));
  stepsLayout->addWidget(m_stepList, 1);

  auto *stepButtons = new QHBoxLayout();
  stepButtons->setSpacing(4);
  m_addStepBtn = new QPushButton(tr("Adicionar passo"), stepsGroup);
  m_addStepBtn->setToolTip(tr("Adiciona um passo no fim da lista."));
  m_editStepBtn = new QPushButton(tr("Editar"), stepsGroup);
  m_removeStepBtn = new QPushButton(tr("Remover"), stepsGroup);
  m_upBtn = new QPushButton(tr("Mover para cima"), stepsGroup);
  m_downBtn = new QPushButton(tr("Mover para baixo"), stepsGroup);
  m_runBtn = new QPushButton(tr("Testar agora"), stepsGroup);
  m_runBtn->setToolTip(
      tr("Executa agora os passos da suíte selecionada, sem precisar gravar."));
  stepButtons->addWidget(m_addStepBtn);
  stepButtons->addWidget(m_editStepBtn);
  stepButtons->addWidget(m_removeStepBtn);
  stepButtons->addWidget(m_upBtn);
  stepButtons->addWidget(m_downBtn);
  stepButtons->addStretch(1);
  stepButtons->addWidget(m_runBtn);
  stepsLayout->addLayout(stepButtons);
  root->addWidget(stepsGroup, 2);

  auto *footer = new QLabel(
      tr("Atalhos de teclado em Configurações → Atalhos: \"Suítes: executar a "
         "suíte ativa\" e uma linha para cada suíte, no formato \"Suítes: "
         "executar «nome»\". Assim uma tecla roda uma automação e outra tecla "
         "roda outra."),
      this);
  footer->setWordWrap(true);
  footer->setEnabled(false);
  root->addWidget(footer);

  connect(m_store, &SuiteStore::changed, this, &SuiteDock::refresh);
  connect(m_suiteList, &QListWidget::itemSelectionChanged, this,
          &SuiteDock::onSelectionChanged);
  connect(m_suiteList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onActivateSelected(); });
  connect(addBtn, &QPushButton::clicked, this, &SuiteDock::onAddSuite);
  connect(m_renameBtn, &QPushButton::clicked, this, &SuiteDock::onRenameSuite);
  connect(m_duplicateBtn, &QPushButton::clicked, this,
          &SuiteDock::onDuplicateSuite);
  connect(m_removeBtn, &QPushButton::clicked, this, &SuiteDock::onRemoveSuite);
  connect(m_mergeBtn, &QPushButton::clicked, this, &SuiteDock::onMergeSelected);
  connect(m_suiteList, &QListWidget::customContextMenuRequested, this,
          &SuiteDock::onSuiteContextMenu);
  connect(m_activateBtn, &QPushButton::clicked, this,
          &SuiteDock::onActivateSelected);
  connect(m_deactivateBtn, &QPushButton::clicked, this,
          &SuiteDock::onClearActive);
  connect(m_openFolderCheck, &QCheckBox::toggled, this,
          &SuiteDock::onOpenFolderToggled);
  connect(m_addStepBtn, &QPushButton::clicked, this, &SuiteDock::onAddStep);
  connect(m_editStepBtn, &QPushButton::clicked, this, &SuiteDock::onEditStep);
  connect(m_stepList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onEditStep(); });
  connect(m_stepList, &QListWidget::itemSelectionChanged, this,
          &SuiteDock::updateButtonStates);
  connect(m_removeStepBtn, &QPushButton::clicked, this,
          &SuiteDock::onRemoveStep);
  connect(m_upBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepUp);
  connect(m_downBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepDown);
  connect(m_runBtn, &QPushButton::clicked, this, &SuiteDock::onRunNow);

  refresh();
}

QString SuiteDock::selectedSuiteId() const
{
  auto *item = m_suiteList->currentItem();
  if (!item)
    return {};
  return item->data(Qt::UserRole).toString();
}

QStringList SuiteDock::selectedSuiteIds() const
{
  QStringList ids;
  for (int i = 0; i < m_suiteList->count(); ++i) {
    QListWidgetItem *item = m_suiteList->item(i);
    if (item->isSelected())
      ids.append(item->data(Qt::UserRole).toString());
  }
  return ids;
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

void SuiteDock::selectSuiteById(const QString &id)
{
  for (int i = 0; i < m_suiteList->count(); ++i) {
    if (m_suiteList->item(i)->data(Qt::UserRole).toString() == id) {
      m_suiteList->setCurrentRow(i);
      return;
    }
  }
}

void SuiteDock::updateButtonStates()
{
  const bool hasSuite = !selectedSuiteId().isEmpty();
  const bool hasActive = m_store && !m_store->activeSuiteId().isEmpty();
  const int stepRow = m_stepList->currentRow();
  const int stepCount = m_stepList->count();
  const bool hasStep = hasSuite && stepRow >= 0;

  m_renameBtn->setEnabled(hasSuite);
  m_duplicateBtn->setEnabled(hasSuite);
  m_removeBtn->setEnabled(hasSuite);
  m_mergeBtn->setEnabled(selectedSuiteIds().size() >= 2);
  m_activateBtn->setEnabled(hasSuite);
  m_deactivateBtn->setEnabled(hasActive);
  m_openFolderCheck->setEnabled(hasSuite);
  m_addStepBtn->setEnabled(hasSuite);
  m_editStepBtn->setEnabled(hasStep);
  m_removeStepBtn->setEnabled(hasStep);
  m_upBtn->setEnabled(hasStep && stepRow > 0);
  m_downBtn->setEnabled(hasStep && stepRow < stepCount - 1);
  m_runBtn->setEnabled(hasSuite || hasActive);
}

void SuiteDock::refresh()
{
  if (!m_store)
    return;

  m_updating = true;
  const QString previous = selectedSuiteId();
  const int previousStepRow = m_stepList->currentRow();
  m_suiteList->clear();

  for (const Suite &suite : m_store->suites()) {
    const bool active = suite.id == m_store->activeSuiteId();
    QString text = tr("%1  —  %2")
                       .arg(suite.name, stepCountText(suite.steps.size()));
    if (active)
      text += tr("  —  ativa");
    auto *item = new QListWidgetItem(text, m_suiteList);
    item->setData(Qt::UserRole, suite.id);
    if (active) {
      QFont font = item->font();
      font.setBold(true);
      item->setFont(font);
      item->setToolTip(tr("Esta é a suíte que roda ao iniciar a gravação."));
    }
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
  m_activeLabel->setText(active ? tr("Suíte ativa: %1").arg(active->name)
                                : tr("Nenhuma suíte ativa"));

  m_updating = false;
  onSelectionChanged();

  if (previousStepRow >= 0 && previousStepRow < m_stepList->count())
    m_stepList->setCurrentRow(previousStepRow);
  updateButtonStates();
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
    m_openFolderCheck->blockSignals(false);
    updateButtonStates();
    return;
  }

  m_openFolderCheck->setChecked(suite->openRecordingFolderOnStop);
  m_openFolderCheck->blockSignals(false);

  int index = 1;
  for (const SuiteStep &step : suite->steps) {
    auto *item = new QListWidgetItem(
        tr("%1. %2 — %3")
            .arg(index++)
            .arg(stepTypeLabel(step.type), step.summary()),
        m_stepList);
    item->setToolTip(stepTypeHelp(step.type));
  }

  if (suite->steps.isEmpty()) {
    auto *item = new QListWidgetItem(
        tr("Nenhum passo ainda. Use \"Adicionar passo\"."), m_stepList);
    item->setFlags(Qt::NoItemFlags);
  }

  updateButtonStates();
}

void SuiteDock::onAddSuite()
{
  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Nova suíte"), tr("Nome do jogo ou da situação:"),
      QLineEdit::Normal, tr("Nova suíte"), &ok);
  if (!ok || name.trimmed().isEmpty())
    return;
  const Suite created = m_store->createSuite(name);
  refresh();
  selectSuiteById(created.id);
}

void SuiteDock::onRenameSuite()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  bool ok = false;
  const QString name =
      QInputDialog::getText(this, tr("Renomear suíte"), tr("Novo nome:"),
                            QLineEdit::Normal, suite->name, &ok);
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
  selectSuiteById(copy.id);
}

void SuiteDock::onRemoveSuite()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  const auto answer = QMessageBox::question(
      this, tr("Excluir suíte"),
      tr("Excluir a suíte \"%1\" e todos os seus passos?").arg(suite->name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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

void SuiteDock::onMergeSelected()
{
  const QStringList ids = selectedSuiteIds();
  if (ids.size() < 2) {
    QMessageBox::information(
        this, tr("Mesclar suítes"),
        tr("Marque pelo menos duas suítes para mesclar.\n\nClique na primeira, "
           "segure Shift e clique na última, ou use Ctrl para escolher uma a "
           "uma."));
    return;
  }

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Mesclar suítes"));
  dialog.setMinimumWidth(430);
  auto *layout = new QVBoxLayout(&dialog);
  layout->setSpacing(10);

  QStringList lines;
  int total = 0;
  for (int i = 0; i < ids.size(); ++i) {
    const Suite *suite = m_store->suiteById(ids.at(i));
    if (!suite)
      continue;
    total += suite->steps.size();
    lines.append(tr("%1. %2 (%3)")
                     .arg(i + 1)
                     .arg(suite->name, stepCountText(suite->steps.size())));
  }

  auto *orderLabel = new QLabel(
      tr("Os passos entram nesta ordem, um depois do outro:\n\n%1\n\nTotal: %2.")
          .arg(lines.join(QStringLiteral("\n")), stepCountText(total)),
      &dialog);
  orderLabel->setWordWrap(true);
  layout->addWidget(orderLabel);

  auto *conditionHint = new QLabel(
      tr("Atenção com as condições: uma condição só afeta o passo logo abaixo "
         "dela. Depois de mesclar, uma condição que estava no fim de uma suíte "
         "passa a valer para o primeiro passo da suíte seguinte."),
      &dialog);
  conditionHint->setWordWrap(true);
  conditionHint->setEnabled(false);
  layout->addWidget(conditionHint);

  auto *form = new QFormLayout();
  auto *nameEdit = new QLineEdit(m_store->suggestedMergeName(ids), &dialog);
  form->addRow(new QLabel(tr("Nome da suíte mesclada:"), &dialog), nameEdit);
  layout->addLayout(form);

  auto *activateCheck =
      new QCheckBox(tr("Ativar a suíte mesclada (ela roda ao gravar)"), &dialog);
  activateCheck->setChecked(true);
  layout->addWidget(activateCheck);

  auto *removeCheck = new QCheckBox(
      tr("Excluir as suítes originais depois de mesclar"), &dialog);
  removeCheck->setToolTip(
      tr("Deixe desmarcado para manter as suítes separadas e ainda ter a "
         "mesclada."));
  layout->addWidget(removeCheck);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Mesclar"));
  buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return;

  const Suite merged =
      m_store->mergeSuites(ids, nameEdit->text(), removeCheck->isChecked());
  if (merged.id.isEmpty()) {
    QMessageBox::warning(this, tr("Mesclar suítes"),
                         tr("Não foi possível mesclar as suítes escolhidas."));
    return;
  }

  if (activateCheck->isChecked())
    m_store->setActiveSuite(merged.id);

  refresh();
  selectSuiteById(merged.id);
}

void SuiteDock::onSuiteContextMenu(const QPoint &pos)
{
  const QStringList ids = selectedSuiteIds();
  QMenu menu(this);

  QAction *activate = menu.addAction(tr("Ativar esta suíte"));
  activate->setEnabled(ids.size() == 1);
  connect(activate, &QAction::triggered, this, &SuiteDock::onActivateSelected);

  QAction *merge = menu.addAction(
      ids.size() >= 2
          ? tr("Mesclar as %1 suítes marcadas...").arg(ids.size())
          : tr("Mesclar suítes marcadas (marque duas ou mais)..."));
  merge->setEnabled(ids.size() >= 2);
  connect(merge, &QAction::triggered, this, &SuiteDock::onMergeSelected);

  menu.addSeparator();

  QAction *rename = menu.addAction(tr("Renomear..."));
  rename->setEnabled(ids.size() == 1);
  connect(rename, &QAction::triggered, this, &SuiteDock::onRenameSuite);

  QAction *duplicate = menu.addAction(tr("Duplicar"));
  duplicate->setEnabled(ids.size() == 1);
  connect(duplicate, &QAction::triggered, this, &SuiteDock::onDuplicateSuite);

  QAction *remove = menu.addAction(tr("Excluir"));
  remove->setEnabled(ids.size() == 1);
  connect(remove, &QAction::triggered, this, &SuiteDock::onRemoveSuite);

  menu.exec(m_suiteList->viewport()->mapToGlobal(pos));
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
  dialog.setMinimumWidth(460);

  auto *root = new QVBoxLayout(&dialog);
  root->setSpacing(10);

  auto *typeForm = new QFormLayout();
  auto *typeCombo = new QComboBox(&dialog);
  typeCombo->addItem(stepTypeLabel(SuiteStepType::SetScene),
                     static_cast<int>(SuiteStepType::SetScene));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::SetTransition),
                     static_cast<int>(SuiteStepType::SetTransition));
  typeCombo->insertSeparator(typeCombo->count());
  typeCombo->addItem(stepTypeLabel(SuiteStepType::SetSourceVisible),
                     static_cast<int>(SuiteStepType::SetSourceVisible));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::RestartMedia),
                     static_cast<int>(SuiteStepType::RestartMedia));
  typeCombo->insertSeparator(typeCombo->count());
  typeCombo->addItem(stepTypeLabel(SuiteStepType::SetMute),
                     static_cast<int>(SuiteStepType::SetMute));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::SetVolume),
                     static_cast<int>(SuiteStepType::SetVolume));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::AudioFade),
                     static_cast<int>(SuiteStepType::AudioFade));
  typeCombo->insertSeparator(typeCombo->count());
  typeCombo->addItem(stepTypeLabel(SuiteStepType::DelayMs),
                     static_cast<int>(SuiteStepType::DelayMs));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::IfCurrentScene),
                     static_cast<int>(SuiteStepType::IfCurrentScene));
  typeCombo->addItem(stepTypeLabel(SuiteStepType::IfSourceVisible),
                     static_cast<int>(SuiteStepType::IfSourceVisible));
  typeCombo->insertSeparator(typeCombo->count());
  typeCombo->addItem(stepTypeLabel(SuiteStepType::OpenUrl),
                     static_cast<int>(SuiteStepType::OpenUrl));
  typeForm->addRow(new QLabel(tr("O que este passo faz:"), &dialog), typeCombo);
  root->addLayout(typeForm);

  auto *helpLabel = new QLabel(&dialog);
  helpLabel->setWordWrap(true);
  helpLabel->setEnabled(false);
  root->addWidget(helpLabel);

  auto *line = new QFrame(&dialog);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  root->addWidget(line);

  auto *form = new QFormLayout();
  form->setSpacing(8);
  root->addLayout(form);

  auto addRow = [&](const QString &text, QWidget *field) {
    auto *label = new QLabel(text, &dialog);
    label->setBuddy(field);
    form->addRow(label, field);
    return label;
  };

  auto *sceneCombo = new QComboBox(&dialog);
  sceneCombo->setEditable(true);
  sceneCombo->addItems(SuiteActions::sceneNames());
  auto *sceneLabel = addRow(tr("Cena:"), sceneCombo);

  auto *sourceCombo = new QComboBox(&dialog);
  sourceCombo->setEditable(true);
  auto *sourceLabel = addRow(tr("Fonte:"), sourceCombo);

  auto *visibleCombo = new QComboBox(&dialog);
  visibleCombo->addItem(tr("Mostrar (deixar visível)"), true);
  visibleCombo->addItem(tr("Ocultar (deixar invisível)"), false);
  auto *visibleLabel = addRow(tr("Ação na fonte:"), visibleCombo);

  auto *expectCombo = new QComboBox(&dialog);
  expectCombo->addItem(tr("Só continua se estiver visível"), true);
  expectCombo->addItem(tr("Só continua se estiver oculta"), false);
  auto *expectLabel = addRow(tr("Condição:"), expectCombo);

  auto *muteCombo = new QComboBox(&dialog);
  muteCombo->addItem(tr("Silenciar (deixar no mudo)"), true);
  muteCombo->addItem(tr("Reativar o som (tirar do mudo)"), false);
  auto *muteLabel = addRow(tr("Ação no áudio:"), muteCombo);

  auto *fadeDirCombo = new QComboBox(&dialog);
  fadeDirCombo->addItem(tr("Fade in — subir do silêncio até o volume"), true);
  fadeDirCombo->addItem(tr("Fade out — descer até o silêncio"), false);
  auto *fadeDirLabel = addRow(tr("Tipo de transição:"), fadeDirCombo);

  auto *fadeCheck = new QCheckBox(
      tr("Aplicar transição de áudio (fade) nesta fonte"), &dialog);
  fadeCheck->setToolTip(
      tr("Disponível porque esta fonte tem áudio. Sem isso, a mudança acontece "
         "de uma vez."));
  auto *fadeCheckLabel = addRow(QString(), fadeCheck);

  auto *volumeSpin = new QDoubleSpinBox(&dialog);
  volumeSpin->setRange(0.0, 100.0);
  volumeSpin->setSuffix(QStringLiteral(" %"));
  volumeSpin->setDecimals(1);
  auto *volumeLabel = addRow(tr("Volume (0% = sem som):"), volumeSpin);

  auto *fadeMsSpin = new QSpinBox(&dialog);
  fadeMsSpin->setRange(0, 60000);
  fadeMsSpin->setSuffix(QStringLiteral(" ms"));
  fadeMsSpin->setSingleStep(100);
  auto *fadeMsLabel = addRow(tr("Duração do fade (1000 ms = 1 segundo):"),
                             fadeMsSpin);

  auto *waitFadeCheck = new QCheckBox(
      tr("Esperar o fade terminar antes do próximo passo"), &dialog);
  waitFadeCheck->setToolTip(
      tr("Desmarque para o fade continuar em segundo plano enquanto os passos "
         "seguintes rodam."));
  auto *waitFadeLabel = addRow(QString(), waitFadeCheck);

  auto *transitionCombo = new QComboBox(&dialog);
  transitionCombo->setEditable(true);
  transitionCombo->addItem(QString());
  transitionCombo->addItems(SuiteActions::transitionNames());
  auto *transitionLabel = addRow(tr("Transição:"), transitionCombo);

  auto *transitionSpin = new QSpinBox(&dialog);
  transitionSpin->setRange(0, 60000);
  transitionSpin->setSuffix(QStringLiteral(" ms"));
  transitionSpin->setSingleStep(50);
  auto *transitionSpinLabel = addRow(tr("Duração da transição:"), transitionSpin);

  auto *delaySpin = new QSpinBox(&dialog);
  delaySpin->setRange(0, 600000);
  delaySpin->setSuffix(QStringLiteral(" ms"));
  delaySpin->setSingleStep(100);
  auto *delayLabel = addRow(tr("Esperar (1000 ms = 1 segundo):"), delaySpin);

  auto *urlEdit = new QLineEdit(&dialog);
  urlEdit->setPlaceholderText(tr("https://exemplo.com ou D:\\Videos"));
  auto *urlLabel = addRow(tr("Site, pasta ou programa:"), urlEdit);

  auto setRowVisible = [](QLabel *label, QWidget *field, bool visible) {
    label->setVisible(visible);
    field->setVisible(visible);
  };

  auto fillSources = [&](SuiteStepType type) {
    const QString current = sourceCombo->currentText();
    sourceCombo->blockSignals(true);
    sourceCombo->clear();
    QStringList names;
    if (type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::AudioFade) {
      names = SuiteActions::audioSourceNames();
      for (const QString &name :
           SuiteActions::sourceNamesInScene(sceneCombo->currentText())) {
        if (!names.contains(name))
          names.append(name);
      }
    } else if (type == SuiteStepType::RestartMedia) {
      names = SuiteActions::sourceNamesInScene(sceneCombo->currentText());
      if (names.isEmpty())
        names = SuiteActions::audioSourceNames();
    } else {
      names = SuiteActions::sourceNamesInScene(sceneCombo->currentText());
    }
    sourceCombo->addItems(names);
    if (!current.isEmpty())
      sourceCombo->setCurrentText(current);
    sourceCombo->blockSignals(false);
  };

  auto applyType = [&]() {
    const auto type =
        static_cast<SuiteStepType>(typeCombo->currentData().toInt());

    const bool needScene = type == SuiteStepType::SetScene ||
                           type == SuiteStepType::SetSourceVisible ||
                           type == SuiteStepType::IfCurrentScene ||
                           type == SuiteStepType::IfSourceVisible;
    const bool needSource = type == SuiteStepType::SetSourceVisible ||
                            type == SuiteStepType::IfSourceVisible ||
                            type == SuiteStepType::SetMute ||
                            type == SuiteStepType::SetVolume ||
                            type == SuiteStepType::AudioFade ||
                            type == SuiteStepType::RestartMedia;
    const bool needTransition = type == SuiteStepType::SetScene ||
                                type == SuiteStepType::SetTransition;

    // O fade só é oferecido quando a fonte escolhida realmente tem áudio.
    const bool hasAudio =
        needSource && SuiteActions::sourceHasAudio(sourceCombo->currentText());
    const bool canOfferFade = hasAudio &&
                              (type == SuiteStepType::SetVolume ||
                               type == SuiteStepType::SetSourceVisible);
    const bool fadeOn = type == SuiteStepType::AudioFade ||
                        (canOfferFade && fadeCheck->isChecked());
    const bool fadeInSelected =
        type == SuiteStepType::AudioFade ? fadeDirCombo->currentData().toBool()
                                         : true;
    const bool needVolume =
        type == SuiteStepType::SetVolume ||
        (type == SuiteStepType::AudioFade && fadeInSelected) ||
        (type == SuiteStepType::SetSourceVisible && fadeOn &&
         visibleCombo->currentData().toBool());

    helpLabel->setText(stepTypeHelp(type));

    setRowVisible(sceneLabel, sceneCombo, needScene);
    setRowVisible(sourceLabel, sourceCombo, needSource);
    setRowVisible(visibleLabel, visibleCombo,
                  type == SuiteStepType::SetSourceVisible);
    setRowVisible(expectLabel, expectCombo,
                  type == SuiteStepType::IfSourceVisible);
    setRowVisible(muteLabel, muteCombo, type == SuiteStepType::SetMute);
    setRowVisible(fadeDirLabel, fadeDirCombo, type == SuiteStepType::AudioFade);
    setRowVisible(fadeCheckLabel, fadeCheck, canOfferFade);
    setRowVisible(volumeLabel, volumeSpin, needVolume);
    setRowVisible(fadeMsLabel, fadeMsSpin, fadeOn);
    setRowVisible(waitFadeLabel, waitFadeCheck, fadeOn);
    setRowVisible(transitionLabel, transitionCombo, needTransition);
    setRowVisible(transitionSpinLabel, transitionSpin, needTransition);
    setRowVisible(delayLabel, delaySpin, type == SuiteStepType::DelayMs);
    setRowVisible(urlLabel, urlEdit, type == SuiteStepType::OpenUrl);

    if (type == SuiteStepType::SetVolume)
      volumeLabel->setText(tr("Volume (0% = sem som):"));
    else
      volumeLabel->setText(tr("Volume no fim do fade:"));

    if (type == SuiteStepType::SetScene)
      sceneLabel->setText(tr("Ir para a cena:"));
    else if (type == SuiteStepType::IfCurrentScene)
      sceneLabel->setText(tr("Cena que precisa estar no ar:"));
    else
      sceneLabel->setText(tr("Cena onde está a fonte:"));

    if (type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::AudioFade)
      sourceLabel->setText(tr("Fonte de áudio:"));
    else if (type == SuiteStepType::RestartMedia)
      sourceLabel->setText(tr("Fonte de mídia:"));
    else
      sourceLabel->setText(tr("Fonte:"));

    if (needSource)
      fillSources(type);

    dialog.adjustSize();
  };

  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(sceneCombo, &QComboBox::currentTextChanged, &dialog,
          [&](const QString &) { applyType(); });
  // A fonte escolhida define se as opções de fade aparecem ou não.
  connect(sourceCombo, &QComboBox::currentTextChanged, &dialog,
          [&](const QString &) { applyType(); });
  connect(visibleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(fadeDirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(fadeCheck, &QCheckBox::toggled, &dialog,
          [&](bool) { applyType(); });

  for (int i = 0; i < typeCombo->count(); ++i) {
    if (typeCombo->itemData(i).isValid() &&
        static_cast<SuiteStepType>(typeCombo->itemData(i).toInt()) == step.type) {
      typeCombo->setCurrentIndex(i);
      break;
    }
  }
  sceneCombo->setCurrentText(step.scene);
  transitionCombo->setCurrentText(step.transition);
  transitionSpin->setValue(step.transitionMs);
  delaySpin->setValue(step.ms);
  visibleCombo->setCurrentIndex(step.visible ? 0 : 1);
  expectCombo->setCurrentIndex(step.visible ? 0 : 1);
  muteCombo->setCurrentIndex(step.muted ? 0 : 1);
  volumeSpin->setValue(step.volume * 100.0);
  fadeDirCombo->setCurrentIndex(step.fadeIn ? 0 : 1);
  fadeCheck->setChecked(step.fadeAudio);
  fadeMsSpin->setValue(step.fadeMs);
  waitFadeCheck->setChecked(step.waitForFade);
  urlEdit->setText(step.url);
  applyType();
  sourceCombo->setCurrentText(step.source);
  applyType();

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText(tr("Salvar"));
  buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return false;

  const auto type = static_cast<SuiteStepType>(typeCombo->currentData().toInt());
  step.type = type;
  step.transitionMs = transitionSpin->value();
  step.ms = delaySpin->value();
  step.muted = muteCombo->currentData().toBool();
  step.volume = volumeSpin->value() / 100.0;
  step.url = urlEdit->text().trimmed();
  step.scene = sceneCombo->currentText().trimmed();
  step.source = sourceCombo->currentText().trimmed();
  step.transition = transitionCombo->currentText().trimmed();
  step.visible = type == SuiteStepType::IfSourceVisible
                     ? expectCombo->currentData().toBool()
                     : visibleCombo->currentData().toBool();
  step.fadeIn = type == SuiteStepType::AudioFade
                    ? fadeDirCombo->currentData().toBool()
                    : step.visible;
  step.fadeMs = fadeMsSpin->value();
  step.waitForFade = waitFadeCheck->isChecked();
  step.fadeAudio = fadeCheck->isChecked() &&
                   SuiteActions::sourceHasAudio(step.source) &&
                   (type == SuiteStepType::SetVolume ||
                    type == SuiteStepType::SetSourceVisible);
  return true;
}

void SuiteDock::onAddStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  SuiteStep step;
  step.type = SuiteStepType::SetScene;
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
  if (!m_engine)
    return;
  const QString id = selectedSuiteId();
  if (id.isEmpty())
    m_engine->runActiveNow();
  else
    m_engine->runSuiteById(id);
}
