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
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>

namespace {

// Cada linha da lista de passos guarda o indice do passo (-1 no cabecalho de
// um grupo) e, quando faz parte de um grupo, o id do grupo.
constexpr int kRoleStepIndex = Qt::UserRole;
constexpr int kRoleGroupId = Qt::UserRole + 1;

// Inicio do bloco a que o passo pertence (o proprio passo, se estiver solto).
int blockStart(const QVector<SuiteStep> &steps, int index)
{
  const QString groupId = steps.at(index).groupId;
  if (groupId.isEmpty())
    return index;
  int start = index;
  while (start > 0 && steps.at(start - 1).groupId == groupId)
    --start;
  return start;
}

// Fim do bloco, exclusivo.
int blockEnd(const QVector<SuiteStep> &steps, int index)
{
  const QString groupId = steps.at(index).groupId;
  if (groupId.isEmpty())
    return index + 1;
  int end = index + 1;
  while (end < steps.size() && steps.at(end).groupId == groupId)
    ++end;
  return end;
}

void moveRange(QVector<SuiteStep> &steps, int start, int end, int destStart)
{
  const QVector<SuiteStep> block = steps.mid(start, end - start);
  for (int i = end - 1; i >= start; --i)
    steps.removeAt(i);
  for (int i = 0; i < block.size(); ++i)
    steps.insert(destStart + i, block.at(i));
}

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
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  m_activeLabel = new QLabel(this);
  QFont activeFont = m_activeLabel->font();
  activeFont.setBold(true);
  m_activeLabel->setFont(activeFont);
  m_activeLabel->setWordWrap(true);
  root->addWidget(m_activeLabel);

  // ---- Suítes ----------------------------------------------------------
  auto *suitesGroup = new QGroupBox(tr("Suítes"), this);
  auto *suitesLayout = new QVBoxLayout(suitesGroup);
  suitesLayout->setContentsMargins(8, 6, 8, 8);
  suitesLayout->setSpacing(5);

  m_suiteList = new QListWidget(suitesGroup);
  m_suiteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_suiteList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_suiteList->setWordWrap(true);
  m_suiteList->setTextElideMode(Qt::ElideNone);
  m_suiteList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_suiteList->setUniformItemSizes(false);
  m_suiteList->setSpacing(1);
  m_suiteList->setMinimumHeight(90);
  m_suiteList->setToolTip(
      tr("Clique duas vezes para ativar a suíte. Shift ou Ctrl marcam várias "
         "para mesclar. O botão direito abre as opções."));
  suitesLayout->addWidget(m_suiteList, 1);

  auto *suiteButtons = new QGridLayout();
  suiteButtons->setSpacing(4);
  auto *addBtn = new QPushButton(tr("Nova"), suitesGroup);
  addBtn->setToolTip(tr("Cria uma suíte vazia."));
  m_renameBtn = new QPushButton(tr("Renomear"), suitesGroup);
  m_renameBtn->setToolTip(tr("Muda o nome da suíte selecionada."));
  m_duplicateBtn = new QPushButton(tr("Duplicar"), suitesGroup);
  m_duplicateBtn->setToolTip(
      tr("Cria uma cópia da suíte selecionada, com todos os passos."));
  m_removeBtn = new QPushButton(tr("Excluir"), suitesGroup);
  m_removeBtn->setToolTip(tr("Apaga a suíte selecionada."));
  m_activateBtn = new QPushButton(tr("Ativar"), suitesGroup);
  m_activateBtn->setToolTip(
      tr("Torna a suíte selecionada a única ativa: é ela que roda ao gravar."));
  m_deactivateBtn = new QPushButton(tr("Desativar"), suitesGroup);
  m_deactivateBtn->setToolTip(
      tr("Nenhuma suíte roda ao iniciar a gravação."));
  m_mergeBtn = new QPushButton(tr("Mesclar..."), suitesGroup);
  m_mergeBtn->setToolTip(
      tr("Junta os passos de várias suítes em uma suíte nova."));
  suiteButtons->addWidget(addBtn, 0, 0);
  suiteButtons->addWidget(m_renameBtn, 0, 1);
  suiteButtons->addWidget(m_duplicateBtn, 0, 2);
  suiteButtons->addWidget(m_removeBtn, 0, 3);
  suiteButtons->addWidget(m_activateBtn, 1, 0);
  suiteButtons->addWidget(m_deactivateBtn, 1, 1);
  suiteButtons->addWidget(m_mergeBtn, 1, 2, 1, 2);
  suitesLayout->addLayout(suiteButtons);
  root->addWidget(suitesGroup, 1);

  // ---- Passos ----------------------------------------------------------
  auto *stepsGroup = new QGroupBox(tr("Suíte selecionada"), this);
  auto *stepsLayout = new QVBoxLayout(stepsGroup);
  stepsLayout->setContentsMargins(8, 6, 8, 8);
  stepsLayout->setSpacing(5);

  m_openFolderCheck = new QCheckBox(
      tr("Ao terminar a gravação, abrir a pasta do arquivo"), stepsGroup);
  m_openFolderCheck->setToolTip(
      tr("Ao parar a gravação, o Windows abre a pasta e seleciona o arquivo que "
         "acabou de ser salvo."));
  stepsLayout->addWidget(m_openFolderCheck);

  auto *stepsHint = new QLabel(
      tr("Passos, executados de cima para baixo. Marque vários com Shift e "
         "use \"Mesclar\" para virarem um grupo."),
      stepsGroup);
  stepsHint->setWordWrap(true);
  stepsHint->setEnabled(false);
  stepsLayout->addWidget(stepsHint);

  m_stepList = new QListWidget(stepsGroup);
  // Selecao multipla: e assim que se marcam os passos para mesclar.
  m_stepList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_stepList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_stepList->setAlternatingRowColors(true);
  // Quebra o texto em vez de cortar: cada passo mostra a descricao inteira.
  m_stepList->setWordWrap(true);
  m_stepList->setTextElideMode(Qt::ElideNone);
  m_stepList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_stepList->setUniformItemSizes(false);
  m_stepList->setSpacing(2);
  m_stepList->setMinimumHeight(120);
  m_stepList->setToolTip(tr("Clique duas vezes em um passo para editá-lo."));
  m_stepList->viewport()->installEventFilter(this);
  stepsLayout->addWidget(m_stepList, 1);

  auto *stepButtons = new QGridLayout();
  stepButtons->setSpacing(4);
  m_addStepBtn = new QPushButton(tr("Adicionar"), stepsGroup);
  m_addStepBtn->setToolTip(tr("Adiciona um passo no fim da lista."));
  m_editStepBtn = new QPushButton(tr("Editar"), stepsGroup);
  m_editStepBtn->setToolTip(tr("Abre o passo selecionado para mudar."));
  m_removeStepBtn = new QPushButton(tr("Remover"), stepsGroup);
  m_removeStepBtn->setToolTip(tr("Apaga o passo selecionado."));
  m_upBtn = new QPushButton(tr("↑ Subir"), stepsGroup);
  m_upBtn->setToolTip(tr("Move o passo selecionado uma posição para cima."));
  m_downBtn = new QPushButton(tr("↓ Descer"), stepsGroup);
  m_downBtn->setToolTip(tr("Move o passo selecionado uma posição para baixo."));
  m_mergeStepsBtn = new QPushButton(tr("Mesclar"), stepsGroup);
  m_mergeStepsBtn->setToolTip(
      tr("Junta os passos marcados em um grupo: eles ficam lado a lado e uma "
         "condição antes do grupo vale para todos."));
  m_unmergeStepsBtn = new QPushButton(tr("Desmesclar"), stepsGroup);
  m_unmergeStepsBtn->setToolTip(
      tr("Desfaz o grupo e devolve as ações como passos soltos."));
  m_runBtn = new QPushButton(tr("Testar agora"), stepsGroup);
  m_runBtn->setToolTip(
      tr("Executa agora os passos da suíte selecionada, sem precisar gravar."));
  stepButtons->addWidget(m_addStepBtn, 0, 0);
  stepButtons->addWidget(m_editStepBtn, 0, 1);
  stepButtons->addWidget(m_removeStepBtn, 0, 2);
  stepButtons->addWidget(m_upBtn, 1, 0);
  stepButtons->addWidget(m_downBtn, 1, 1);
  stepButtons->addWidget(m_runBtn, 1, 2);
  stepButtons->addWidget(m_mergeStepsBtn, 2, 0);
  stepButtons->addWidget(m_unmergeStepsBtn, 2, 1, 1, 2);
  stepsLayout->addLayout(stepButtons);
  root->addWidget(stepsGroup, 2);

  auto *footer = new QLabel(
      tr("Atalhos: Configurações → Atalhos, uma linha por suíte."), this);
  footer->setWordWrap(true);
  footer->setEnabled(false);
  footer->setToolTip(
      tr("Além de \"Suítes: executar a suíte ativa\", cada suíte tem a sua "
         "própria linha, no formato \"Suítes: executar «nome»\". Assim uma "
         "tecla roda uma automação e outra tecla roda outra."));
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
  connect(m_mergeBtn, &QPushButton::clicked, this, &SuiteDock::onMergeSuites);
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
  connect(m_mergeStepsBtn, &QPushButton::clicked, this,
          &SuiteDock::onMergeSteps);
  connect(m_unmergeStepsBtn, &QPushButton::clicked, this,
          &SuiteDock::onUnmergeSteps);
  connect(m_stepList, &QListWidget::customContextMenuRequested, this,
          &SuiteDock::onStepContextMenu);
  connect(m_runBtn, &QPushButton::clicked, this, &SuiteDock::onRunNow);

  refresh();
}

bool SuiteDock::eventFilter(QObject *watched, QEvent *event)
{
  if (m_stepList && watched == m_stepList->viewport() &&
      event->type() == QEvent::Resize) {
    // Sem isto, o Qt mantem a altura antiga e a ultima linha fica cortada.
    QTimer::singleShot(0, m_stepList, [this]() {
      if (m_stepList)
        m_stepList->doItemsLayout();
    });
  }
  return QWidget::eventFilter(watched, event);
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

  m_renameBtn->setEnabled(hasSuite);
  m_duplicateBtn->setEnabled(hasSuite);
  m_removeBtn->setEnabled(hasSuite);
  m_mergeBtn->setEnabled(m_store && m_store->suites().size() >= 2);
  m_activateBtn->setEnabled(hasSuite);
  m_deactivateBtn->setEnabled(hasActive);
  m_openFolderCheck->setEnabled(hasSuite);

  const int stepRow = m_stepList->currentRow();
  const int stepIndex = currentStepIndex();
  const bool onHeader = stepIndex < 0 && !currentRowGroupId().isEmpty();
  const bool hasRow = hasSuite && stepRow >= 0 && (stepIndex >= 0 || onHeader);
  const int markedSteps = markedStepIndices().size();
  const Suite *suite = m_store ? m_store->suiteById(selectedSuiteId()) : nullptr;
  bool rowInGroup = onHeader;
  if (!rowInGroup && suite && stepIndex >= 0 && stepIndex < suite->steps.size())
    rowInGroup = !suite->steps.at(stepIndex).groupId.isEmpty();

  m_addStepBtn->setEnabled(hasSuite);
  m_editStepBtn->setEnabled(hasRow);
  m_removeStepBtn->setEnabled(hasRow);
  m_upBtn->setEnabled(hasRow && stepRow > 0);
  m_downBtn->setEnabled(hasRow && stepRow < m_stepList->count() - 1);
  m_mergeStepsBtn->setText(markedSteps >= 2
                               ? tr("Mesclar (%1)").arg(markedSteps)
                               : tr("Mesclar"));
  m_mergeStepsBtn->setEnabled(markedSteps >= 2);
  m_unmergeStepsBtn->setEnabled(rowInGroup);
  m_runBtn->setEnabled(hasSuite || hasActive);
}

void SuiteDock::refresh()
{
  if (!m_store)
    return;

  m_updating = true;
  // Guarda todas as suites marcadas, e nao so a linha atual, para uma
  // marcacao de varias nao se desfazer a cada atualizacao da lista.
  const QStringList previouslyMarked = selectedSuiteIds();
  const QString previousCurrent = selectedSuiteId();
  const int previousStepIndex = currentStepIndex();
  m_suiteList->clear();

  QListWidgetItem *currentItem = nullptr;
  for (const Suite &suite : m_store->suites()) {
    const bool active = suite.id == m_store->activeSuiteId();
    QString text = tr("%1  ·  %2")
                       .arg(suite.name, stepCountText(suite.steps.size()));
    if (active)
      text += tr("  ·  ativa");
    auto *item = new QListWidgetItem(text, m_suiteList);
    item->setData(Qt::UserRole, suite.id);
    if (active) {
      QFont font = item->font();
      font.setBold(true);
      item->setFont(font);
      item->setToolTip(tr("Esta é a suíte que roda ao iniciar a gravação."));
    }
    if (previouslyMarked.contains(suite.id))
      item->setSelected(true);
    if (suite.id == previousCurrent)
      currentItem = item;
  }

  if (!currentItem && m_suiteList->count() > 0)
    currentItem = m_suiteList->item(0);
  if (currentItem) {
    m_suiteList->setCurrentItem(currentItem,
                                previouslyMarked.isEmpty()
                                    ? QItemSelectionModel::ClearAndSelect
                                    : QItemSelectionModel::NoUpdate);
  }

  const Suite *active = m_store->activeSuite();
  m_activeLabel->setText(active ? tr("Suíte ativa: %1").arg(active->name)
                                : tr("Nenhuma suíte ativa"));

  m_updating = false;
  onSelectionChanged();

  if (previousStepIndex >= 0)
    selectStepRowByIndex(previousStepIndex);
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

  const QVector<SuiteStep> &steps = suite->steps;
  int block = 1;
  int i = 0;
  while (i < steps.size()) {
    const QString groupId = steps.at(i).groupId;

    if (groupId.isEmpty()) {
      const SuiteStep &step = steps.at(i);
      // Primeira linha: numero e tipo. Segunda linha: o que o passo faz.
      auto *item = new QListWidgetItem(
          tr("%1 · %2\n%3")
              .arg(block)
              .arg(stepTypeLabel(step.type), step.summary()),
          m_stepList);
      item->setData(kRoleStepIndex, i);
      item->setToolTip(stepTypeHelp(step.type));
      QFont font = item->font();
      font.setPointSizeF(font.pointSizeF() * 0.95);
      item->setFont(font);
      ++i;
      ++block;
      continue;
    }

    int end = i;
    while (end < steps.size() && steps.at(end).groupId == groupId)
      ++end;
    const int members = end - i;

    const QString name = steps.at(i).groupName.trimmed().isEmpty()
                             ? tr("Grupo sem nome")
                             : steps.at(i).groupName.trimmed();
    auto *header = new QListWidgetItem(
        tr("%1 · Grupo: %2  ·  %3 mescladas, rodam em sequência")
            .arg(block)
            .arg(name)
            .arg(members),
        m_stepList);
    header->setData(kRoleStepIndex, -1);
    header->setData(kRoleGroupId, groupId);
    header->setToolTip(
        tr("Um grupo conta como um bloco só: uma condição colocada antes dele "
           "vale para todas as ações de dentro."));
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);

    for (int k = i; k < end; ++k) {
      const SuiteStep &step = steps.at(k);
      auto *item = new QListWidgetItem(
          tr("      • %1\n      %2")
              .arg(stepTypeLabel(step.type), step.summary()),
          m_stepList);
      item->setData(kRoleStepIndex, k);
      item->setData(kRoleGroupId, groupId);
      item->setToolTip(stepTypeHelp(step.type));
      QFont font = item->font();
      font.setPointSizeF(font.pointSizeF() * 0.95);
      item->setFont(font);
    }

    i = end;
    ++block;
  }

  if (steps.isEmpty()) {
    auto *item = new QListWidgetItem(
        tr("Nenhum passo ainda. Use \"Adicionar\"."), m_stepList);
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

void SuiteDock::onMergeSuites()
{
  if (!m_store)
    return;

  if (m_store->suites().size() < 2) {
    QMessageBox::information(
        this, tr("Mesclar suítes"),
        tr("É preciso ter pelo menos duas suítes para mesclar."));
    return;
  }

  const QStringList marked = selectedSuiteIds();

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Mesclar suítes"));
  dialog.setMinimumWidth(440);
  auto *layout = new QVBoxLayout(&dialog);
  layout->setSpacing(8);

  auto *intro = new QLabel(
      tr("Clique nas caixinhas para escolher as suítes que devem virar uma só. "
         "Os passos entram na ordem desta lista, de cima para baixo."),
      &dialog);
  intro->setWordWrap(true);
  layout->addWidget(intro);

  // Lista com caixas de marcar: nao depende da selecao do painel, entao a
  // mesclagem funciona mesmo se a marcacao lá fora se perder.
  auto *pickList = new QListWidget(&dialog);
  pickList->setSelectionMode(QAbstractItemView::SingleSelection);
  pickList->setWordWrap(true);
  pickList->setTextElideMode(Qt::ElideNone);
  pickList->setMinimumHeight(150);
  for (const Suite &suite : m_store->suites()) {
    auto *item = new QListWidgetItem(
        tr("%1  ·  %2").arg(suite.name, stepCountText(suite.steps.size())),
        pickList);
    item->setData(Qt::UserRole, suite.id);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(marked.contains(suite.id) ? Qt::Checked
                                                  : Qt::Unchecked);
  }
  layout->addWidget(pickList, 1);

  auto *summary = new QLabel(&dialog);
  summary->setWordWrap(true);
  layout->addWidget(summary);

  auto *conditionHint = new QLabel(
      tr("Sobre as condições: cada condição vale só para o passo logo abaixo "
         "dela. Ao mesclar, uma condição que estava no fim de uma suíte passa a "
         "valer para o primeiro passo da suíte seguinte."),
      &dialog);
  conditionHint->setWordWrap(true);
  conditionHint->setEnabled(false);
  layout->addWidget(conditionHint);

  auto *form = new QFormLayout();
  auto *nameEdit = new QLineEdit(&dialog);
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
  QPushButton *okButton = buttons->button(QDialogButtonBox::Ok);
  okButton->setText(tr("Mesclar"));
  buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto checkedIds = [pickList]() {
    QStringList ids;
    for (int i = 0; i < pickList->count(); ++i) {
      QListWidgetItem *item = pickList->item(i);
      if (item->checkState() == Qt::Checked)
        ids.append(item->data(Qt::UserRole).toString());
    }
    return ids;
  };

  bool nameTouched = false;
  connect(nameEdit, &QLineEdit::textEdited, &dialog,
          [&nameTouched](const QString &) { nameTouched = true; });

  auto syncDialog = [&]() {
    const QStringList ids = checkedIds();
    int total = 0;
    for (const QString &id : ids) {
      if (const Suite *suite = m_store->suiteById(id))
        total += suite->steps.size();
    }
    if (ids.size() < 2) {
      summary->setText(tr("Marque pelo menos duas suítes."));
    } else {
      summary->setText(tr("%1 suítes marcadas, %2 no total.")
                           .arg(ids.size())
                           .arg(stepCountText(total)));
    }
    if (!nameTouched)
      nameEdit->setText(m_store->suggestedMergeName(ids));
    okButton->setEnabled(ids.size() >= 2);
  };

  connect(pickList, &QListWidget::itemChanged, &dialog,
          [&syncDialog](QListWidgetItem *) { syncDialog(); });
  syncDialog();

  if (dialog.exec() != QDialog::Accepted)
    return;

  const QStringList ids = checkedIds();
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
  const int marked = selectedSuiteIds().size();
  QMenu menu(this);

  QAction *activate = menu.addAction(tr("Ativar esta suíte"));
  activate->setEnabled(!selectedSuiteId().isEmpty());
  connect(activate, &QAction::triggered, this, &SuiteDock::onActivateSelected);

  QAction *merge = menu.addAction(
      marked >= 2 ? tr("Mesclar as %1 suítes marcadas...").arg(marked)
                  : tr("Mesclar suítes..."));
  connect(merge, &QAction::triggered, this, &SuiteDock::onMergeSuites);

  menu.addSeparator();

  QAction *rename = menu.addAction(tr("Renomear..."));
  rename->setEnabled(!selectedSuiteId().isEmpty());
  connect(rename, &QAction::triggered, this, &SuiteDock::onRenameSuite);

  QAction *duplicate = menu.addAction(tr("Duplicar"));
  duplicate->setEnabled(!selectedSuiteId().isEmpty());
  connect(duplicate, &QAction::triggered, this, &SuiteDock::onDuplicateSuite);

  QAction *remove = menu.addAction(tr("Excluir"));
  remove->setEnabled(!selectedSuiteId().isEmpty());
  connect(remove, &QAction::triggered, this, &SuiteDock::onRemoveSuite);

  menu.exec(m_suiteList->mapToGlobal(pos));
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

int SuiteDock::currentStepIndex() const
{
  QListWidgetItem *item = m_stepList->currentItem();
  if (!item)
    return -1;
  const QVariant value = item->data(kRoleStepIndex);
  return value.isValid() ? value.toInt() : -1;
}

QString SuiteDock::currentRowGroupId() const
{
  QListWidgetItem *item = m_stepList->currentItem();
  return item ? item->data(kRoleGroupId).toString() : QString();
}

QList<int> SuiteDock::markedStepIndices() const
{
  QList<int> indices;
  for (int i = 0; i < m_stepList->count(); ++i) {
    QListWidgetItem *item = m_stepList->item(i);
    if (!item->isSelected())
      continue;
    const QVariant value = item->data(kRoleStepIndex);
    if (!value.isValid())
      continue;
    const int stepIndex = value.toInt();
    if (stepIndex >= 0 && !indices.contains(stepIndex))
      indices.append(stepIndex);
  }
  std::sort(indices.begin(), indices.end());
  return indices;
}

void SuiteDock::selectStepRowByIndex(int stepIndex)
{
  for (int i = 0; i < m_stepList->count(); ++i) {
    const QVariant value = m_stepList->item(i)->data(kRoleStepIndex);
    if (value.isValid() && value.toInt() == stepIndex) {
      m_stepList->setCurrentRow(i);
      return;
    }
  }
}

void SuiteDock::onEditStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;

  const int index = currentStepIndex();
  if (index < 0) {
    // Cabecalho de grupo: editar o grupo significa renomear.
    const QString groupId = currentRowGroupId();
    if (groupId.isEmpty())
      return;
    QString currentName;
    for (const SuiteStep &step : suite->steps) {
      if (step.groupId == groupId) {
        currentName = step.groupName;
        break;
      }
    }
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Renomear grupo"), tr("Nome do grupo:"), QLineEdit::Normal,
        currentName, &ok);
    if (!ok)
      return;
    for (SuiteStep &step : suite->steps) {
      if (step.groupId == groupId)
        step.groupName = name.trimmed();
    }
    persistSelectedSuite();
    return;
  }

  if (index >= suite->steps.size())
    return;
  SuiteStep step = suite->steps.at(index);
  if (!editStepDialog(step, false))
    return;
  suite->steps[index] = step;
  persistSelectedSuite();
}

void SuiteDock::onRemoveStep()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;

  const int index = currentStepIndex();
  if (index < 0) {
    // Cabecalho de grupo: remove o grupo inteiro.
    const QString groupId = currentRowGroupId();
    if (groupId.isEmpty())
      return;
    int members = 0;
    for (const SuiteStep &step : suite->steps) {
      if (step.groupId == groupId)
        ++members;
    }
    const auto answer = QMessageBox::question(
        this, tr("Remover grupo"),
        tr("Remover o grupo e as suas %1 ações?").arg(members),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
      return;
    for (int i = suite->steps.size() - 1; i >= 0; --i) {
      if (suite->steps.at(i).groupId == groupId)
        suite->steps.removeAt(i);
    }
    persistSelectedSuite();
    return;
  }

  if (index >= suite->steps.size())
    return;
  suite->steps.removeAt(index);
  persistSelectedSuite();
}

void SuiteDock::onMoveStepUp()
{
  Suite *suite = selectedSuite();
  if (!suite || suite->steps.isEmpty())
    return;

  QVector<SuiteStep> &steps = suite->steps;
  const int index = currentStepIndex();

  if (index < 0) {
    // Cabecalho: sobe o grupo inteiro por cima do bloco anterior.
    const QString groupId = currentRowGroupId();
    if (groupId.isEmpty())
      return;
    int start = -1;
    for (int i = 0; i < steps.size(); ++i) {
      if (steps.at(i).groupId == groupId) {
        start = i;
        break;
      }
    }
    if (start <= 0)
      return;
    const int end = blockEnd(steps, start);
    const int previousStart = blockStart(steps, start - 1);
    moveRange(steps, start, end, previousStart);
    persistSelectedSuite();
    return;
  }

  if (index <= 0 || index >= steps.size())
    return;

  const QString groupId = steps.at(index).groupId;
  if (!groupId.isEmpty()) {
    // Dentro de um grupo o passo se move so entre os companheiros, para o
    // grupo continuar sendo um bloco unico.
    if (steps.at(index - 1).groupId != groupId)
      return;
    steps.swapItemsAt(index, index - 1);
    persistSelectedSuite();
    selectStepRowByIndex(index - 1);
    return;
  }

  const int previousStart = blockStart(steps, index - 1);
  moveRange(steps, index, index + 1, previousStart);
  persistSelectedSuite();
  selectStepRowByIndex(previousStart);
}

void SuiteDock::onMoveStepDown()
{
  Suite *suite = selectedSuite();
  if (!suite || suite->steps.isEmpty())
    return;

  QVector<SuiteStep> &steps = suite->steps;
  const int index = currentStepIndex();

  if (index < 0) {
    const QString groupId = currentRowGroupId();
    if (groupId.isEmpty())
      return;
    int start = -1;
    for (int i = 0; i < steps.size(); ++i) {
      if (steps.at(i).groupId == groupId) {
        start = i;
        break;
      }
    }
    if (start < 0)
      return;
    const int end = blockEnd(steps, start);
    if (end >= steps.size())
      return;
    const int nextEnd = blockEnd(steps, end);
    moveRange(steps, start, end, nextEnd - (end - start));
    persistSelectedSuite();
    return;
  }

  if (index < 0 || index >= steps.size() - 1)
    return;

  const QString groupId = steps.at(index).groupId;
  if (!groupId.isEmpty()) {
    if (steps.at(index + 1).groupId != groupId)
      return;
    steps.swapItemsAt(index, index + 1);
    persistSelectedSuite();
    selectStepRowByIndex(index + 1);
    return;
  }

  const int nextEnd = blockEnd(steps, index + 1);
  moveRange(steps, index, index + 1, nextEnd - 1);
  persistSelectedSuite();
  selectStepRowByIndex(nextEnd - 1);
}

void SuiteDock::onMergeSteps()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;

  const QList<int> indices = markedStepIndices();
  if (indices.size() < 2) {
    QMessageBox::information(
        this, tr("Mesclar passos"),
        tr("Marque pelo menos dois passos para mesclar.\n\nClique no primeiro, "
           "segure Shift e clique no último, ou use Ctrl para escolher um a "
           "um."));
    return;
  }

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Mesclar passos"),
      tr("Nome do grupo (só para você se organizar):"), QLineEdit::Normal,
      tr("Ações da gravação"), &ok);
  if (!ok)
    return;

  QVector<SuiteStep> picked;
  picked.reserve(indices.size());
  for (int index : indices) {
    if (index >= 0 && index < suite->steps.size())
      picked.append(suite->steps.at(index));
  }
  if (picked.size() < 2)
    return;

  const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  for (SuiteStep &step : picked) {
    step.groupId = groupId;
    step.groupName = name.trimmed();
  }

  // Tira os passos escolhidos de onde estavam e coloca todos juntos, na
  // posicao do primeiro deles.
  const int insertAt = indices.first();
  for (int i = indices.size() - 1; i >= 0; --i) {
    const int index = indices.at(i);
    if (index >= 0 && index < suite->steps.size())
      suite->steps.removeAt(index);
  }
  for (int i = 0; i < picked.size(); ++i)
    suite->steps.insert(insertAt + i, picked.at(i));

  persistSelectedSuite();
  selectStepRowByIndex(insertAt);
}

void SuiteDock::onUnmergeSteps()
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;

  QString groupId = currentRowGroupId();
  if (groupId.isEmpty()) {
    // Tambem aceita desmesclar a partir de qualquer passo marcado.
    for (int index : markedStepIndices()) {
      if (index >= 0 && index < suite->steps.size() &&
          !suite->steps.at(index).groupId.isEmpty()) {
        groupId = suite->steps.at(index).groupId;
        break;
      }
    }
  }
  if (groupId.isEmpty()) {
    QMessageBox::information(
        this, tr("Desmesclar"),
        tr("Escolha um grupo (ou um passo de dentro de um grupo) para "
           "desmesclar."));
    return;
  }

  int firstIndex = -1;
  for (int i = 0; i < suite->steps.size(); ++i) {
    if (suite->steps.at(i).groupId != groupId)
      continue;
    if (firstIndex < 0)
      firstIndex = i;
    suite->steps[i].groupId.clear();
    suite->steps[i].groupName.clear();
  }
  persistSelectedSuite();
  if (firstIndex >= 0)
    selectStepRowByIndex(firstIndex);
}

void SuiteDock::onStepContextMenu(const QPoint &pos)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;

  const QList<int> marked = markedStepIndices();
  const int index = currentStepIndex();
  const bool onHeader = index < 0 && !currentRowGroupId().isEmpty();
  const bool inGroup =
      onHeader || (index >= 0 && index < suite->steps.size() &&
                   !suite->steps.at(index).groupId.isEmpty());

  QMenu menu(this);

  QAction *merge = menu.addAction(
      marked.size() >= 2
          ? tr("Mesclar os %1 passos marcados...").arg(marked.size())
          : tr("Mesclar passos (marque dois ou mais com Shift)..."));
  merge->setEnabled(marked.size() >= 2);
  connect(merge, &QAction::triggered, this, &SuiteDock::onMergeSteps);

  QAction *unmerge = menu.addAction(tr("Desmesclar este grupo"));
  unmerge->setEnabled(inGroup);
  connect(unmerge, &QAction::triggered, this, &SuiteDock::onUnmergeSteps);

  menu.addSeparator();

  QAction *edit =
      menu.addAction(onHeader ? tr("Renomear grupo...") : tr("Editar passo..."));
  edit->setEnabled(index >= 0 || onHeader);
  connect(edit, &QAction::triggered, this, &SuiteDock::onEditStep);

  QAction *remove =
      menu.addAction(onHeader ? tr("Remover o grupo inteiro") : tr("Remover"));
  remove->setEnabled(index >= 0 || onHeader);
  connect(remove, &QAction::triggered, this, &SuiteDock::onRemoveStep);

  menu.addSeparator();

  QAction *up = menu.addAction(tr("Subir"));
  up->setEnabled(index >= 0 || onHeader);
  connect(up, &QAction::triggered, this, &SuiteDock::onMoveStepUp);

  QAction *down = menu.addAction(tr("Descer"));
  down->setEnabled(index >= 0 || onHeader);
  connect(down, &QAction::triggered, this, &SuiteDock::onMoveStepDown);

  menu.exec(m_stepList->mapToGlobal(pos));
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
