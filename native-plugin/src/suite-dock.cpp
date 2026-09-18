#include "suite-dock.hpp"
#include "suite-actions.hpp"
#include "suite-outro-hotkey.hpp"
#include "suite-types.hpp"

#include <obs-module.h>

#include <QAbstractItemView>
#include <QAction>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QVariant>

#include <algorithm>
#include <cmath>

namespace {

// Cada linha da lista de passos guarda o indice do passo (-1 no cabecalho de
// um grupo) e, quando faz parte de um grupo, o id do grupo.
constexpr int kRoleStepIndex = Qt::UserRole;
constexpr int kRoleGroupId = Qt::UserRole + 1;

enum class StepCategory {
  Scene,
  Source,
  Audio,
  Screen,
  Wait,
  Condition,
  Open,
};

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

QString stepTypeIcon(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
  case SuiteStepType::SetTransition:
    return QStringLiteral("🎬");
  case SuiteStepType::SetSourceVisible:
  case SuiteStepType::RestartMedia:
    return QStringLiteral("👁");
  case SuiteStepType::SetMute:
  case SuiteStepType::SetVolume:
  case SuiteStepType::AudioFade:
    return QStringLiteral("🔊");
  case SuiteStepType::ScreenFade:
    return QStringLiteral("⬛");
  case SuiteStepType::DelayMs:
    return QStringLiteral("⏱");
  case SuiteStepType::IfCurrentScene:
  case SuiteStepType::IfSourceVisible:
  case SuiteStepType::IfTrigger:
    return QStringLiteral("❖");
  case SuiteStepType::OpenUrl:
    return QStringLiteral("🔗");
  }
  return QString();
}

QString groupWhenBadge(SuiteGroupWhen when)
{
  switch (when) {
  case SuiteGroupWhen::RecordingStarted:
    return QObject::tr("Início");
  case SuiteGroupWhen::RecordingStopped:
    return QObject::tr("Fim");
  case SuiteGroupWhen::Manual:
    return QObject::tr("Manual");
  case SuiteGroupWhen::Always:
    return QObject::tr("Sempre");
  }
  return QString();
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
  case SuiteStepType::ScreenFade:
    return QObject::tr("Escurecer ou clarear a tela toda");
  case SuiteStepType::DelayMs:
    return QObject::tr("Esperar um tempo");
  case SuiteStepType::IfCurrentScene:
    return QObject::tr("Condição: a cena atual é...");
  case SuiteStepType::IfSourceVisible:
    return QObject::tr("Condição: a fonte está visível ou oculta");
  case SuiteStepType::IfTrigger:
    return QObject::tr("Condição: o que disparou a suíte");
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
        "escolhida. A transição da fonte é a mesma do OBS: escolha, por "
        "exemplo, esmaecer, e a fonte aparece ou sai aos poucos, na "
        "velocidade que você definir. Se a fonte tiver áudio, você também pode "
        "pedir fade no som.");
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
  case SuiteStepType::ScreenFade:
    return QObject::tr(
        "Cobre a tela inteira com preto e esmaece. \"Clarear\" começa no preto "
        "e revela a imagem, ideal no início da gravação; \"Escurecer\" leva a "
        "imagem até o preto total, ideal antes de encerrar. O plugin cria "
        "sozinho a fonte \"Suítes: Tela Preta\" na cena e a deixa por cima de "
        "tudo. Este passo sempre espera o esmaecer terminar.");
  case SuiteStepType::DelayMs:
    return QObject::tr(
        "Faz uma pausa antes de executar o próximo passo. Use para dar tempo à "
        "transição ou ao jogo abrir.");
  case SuiteStepType::IfCurrentScene:
    return QObject::tr(
        "Verifica se a cena que está no ar é a escolhida. Se não for, o alvo "
        "da condição (o passo logo abaixo ou o grupo que você escolher) não "
        "roda.");
  case SuiteStepType::IfSourceVisible:
    return QObject::tr(
        "Verifica se a fonte está visível (ou oculta, se você escolher assim). "
        "Se a verificação falhar, o alvo da condição (o passo logo abaixo ou o "
        "grupo que você escolher) não roda.");
  case SuiteStepType::IfTrigger:
    return QObject::tr(
        "Verifica o que fez a suíte rodar: o início da gravação, o fim da "
        "gravação ou o atalho. É assim que a mesma suíte faz uma coisa ao "
        "começar a gravar e outra ao encerrar. Para o fim da gravação "
        "funcionar, deixe marcado \"Rodar ao encerrar a gravação\" na suíte.");
  case SuiteStepType::OpenUrl:
    return QObject::tr(
        "Abre um site, uma pasta do Windows ou um programa. Exemplos: "
        "https://seusite.com ou D:\\Videos.");
  }
  return QString();
}

StepCategory categoryForType(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
  case SuiteStepType::SetTransition:
    return StepCategory::Scene;
  case SuiteStepType::SetSourceVisible:
  case SuiteStepType::RestartMedia:
    return StepCategory::Source;
  case SuiteStepType::SetMute:
  case SuiteStepType::SetVolume:
  case SuiteStepType::AudioFade:
    return StepCategory::Audio;
  case SuiteStepType::ScreenFade:
    return StepCategory::Screen;
  case SuiteStepType::DelayMs:
    return StepCategory::Wait;
  case SuiteStepType::IfCurrentScene:
  case SuiteStepType::IfSourceVisible:
  case SuiteStepType::IfTrigger:
    return StepCategory::Condition;
  case SuiteStepType::OpenUrl:
    return StepCategory::Open;
  }
  return StepCategory::Wait;
}

QString categoryLabel(StepCategory category)
{
  switch (category) {
  case StepCategory::Scene:
    return QObject::tr("Cena");
  case StepCategory::Source:
    return QObject::tr("Fonte");
  case StepCategory::Audio:
    return QObject::tr("Áudio");
  case StepCategory::Screen:
    return QObject::tr("Tela");
  case StepCategory::Wait:
    return QObject::tr("Espera");
  case StepCategory::Condition:
    return QObject::tr("Condição");
  case StepCategory::Open:
    return QObject::tr("Abrir");
  }
  return QObject::tr("Passo");
}

int msFromSeconds(double seconds)
{
  return qMax(0, static_cast<int>(std::lround(seconds * 1000.0)));
}

double secondsFromMs(int ms)
{
  return qMax(0.1, ms / 1000.0);
}

} // namespace

SuiteDock::SuiteDock(SuiteStore *store, SuiteEngine *engine, QWidget *parent)
    : QWidget(parent), m_store(store), m_engine(engine)
{
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(4, 4, 4, 4);
  root->setSpacing(4);

  // ---- Seletor da suíte (o "pai"), sempre visível ----------------------
  auto *suiteRow = new QHBoxLayout();
  suiteRow->setSpacing(4);

  m_suiteTabs = new QTabBar(this);
  m_suiteTabs->setExpanding(false);
  m_suiteTabs->setUsesScrollButtons(true);
  m_suiteTabs->setElideMode(Qt::ElideRight);
  m_suiteTabs->setDrawBase(false);
  m_suiteTabs->setDocumentMode(true);
  m_suiteTabs->setContextMenuPolicy(Qt::CustomContextMenu);
  m_suiteTabs->setToolTip(
      tr("Uma aba por suíte. A aba escolhida manda no que aparece abaixo. "
         "Clique duas vezes para ativar a suíte; o botão direito abre as "
         "opções."));
  suiteRow->addWidget(m_suiteTabs, 1);

  auto *addBtn = new QPushButton(tr("+ Nova suíte"), this);
  addBtn->setToolTip(
      tr("Cria uma suíte nova, que é o \"pai\". Depois de criada, use a aba "
         "Passos para montar a automação dentro dela."));
  suiteRow->addWidget(addBtn);
  root->addLayout(suiteRow);

  // ---- Estado vazio (nenhuma suíte ainda) ------------------------------
  m_emptyState = new QWidget(this);
  auto *emptyLayout = new QVBoxLayout(m_emptyState);
  emptyLayout->setContentsMargins(16, 24, 16, 24);
  emptyLayout->setSpacing(12);
  auto *emptyLabel = new QLabel(
      tr("Nenhuma suíte ainda.\n\nCrie uma suíte vazia ou comece com um "
         "exemplo pronto: intro ao gravar e outro ao encerrar."),
      m_emptyState);
  emptyLabel->setWordWrap(true);
  emptyLabel->setAlignment(Qt::AlignCenter);
  emptyLayout->addStretch(1);
  emptyLayout->addWidget(emptyLabel);
  m_exampleSuiteBtn =
      new QPushButton(tr("Criar suíte de exemplo (intro + outro)"), m_emptyState);
  m_exampleSuiteBtn->setToolTip(
      tr("Cria a suíte \"Gravação padrão\" com um grupo de início (clarear) e "
         "um de encerramento (escurecer). Depois ative-a na aba Suíte."));
  emptyLayout->addWidget(m_exampleSuiteBtn, 0, Qt::AlignHCenter);
  emptyLayout->addStretch(2);
  root->addWidget(m_emptyState, 1);

  // ---- Conteúdo em abas, uma coisa por vez -----------------------------
  auto *pages = new QTabWidget(this);
  pages->setDocumentMode(true);
  m_pages = pages;

  auto *stepsPage = new QWidget(pages);
  auto *stepsLayout = new QVBoxLayout(stepsPage);
  stepsLayout->setContentsMargins(6, 6, 6, 6);
  stepsLayout->setSpacing(4);

  // 1) Barra de execução
  m_runOnStartCheck = new QCheckBox(tr("Rodar ao iniciar"), stepsPage);
  m_runOnStartCheck->setToolTip(
      tr("Quando você aperta para gravar (pelo OBS ou pela tecla de atalho da "
         "gravação), esta suíte roda."));
  m_runOnStopCheck = new QCheckBox(tr("Rodar ao encerrar"), stepsPage);
  m_runOnStopCheck->setToolTip(
      tr("Quando a gravação para, esta suíte roda de novo. Use os slots de "
         "grupo ou a condição \"o que disparou a suíte\" para separar o que "
         "acontece em cada momento."));
  m_openFolderCheck =
      new QCheckBox(tr("Abrir pasta ao terminar"), stepsPage);
  m_openFolderCheck->setToolTip(
      tr("Ao parar a gravação, o Windows abre a pasta e seleciona o arquivo que "
         "acabou de ser salvo."));
  auto *runtimeRow = new QHBoxLayout();
  runtimeRow->setSpacing(8);
  runtimeRow->addWidget(m_runOnStartCheck);
  runtimeRow->addWidget(m_runOnStopCheck);
  runtimeRow->addWidget(m_openFolderCheck);
  runtimeRow->addStretch(1);
  stepsLayout->addLayout(runtimeRow);

  // 2) Slots de grupo
  auto *slotForm = new QFormLayout();
  slotForm->setSpacing(4);
  m_groupOnStartCombo = new QComboBox(stepsPage);
  m_groupOnStartCombo->setToolTip(
      tr("Qual grupo roda ao iniciar a gravação. Vazio = automático, pelo "
         "momento marcado em cada grupo."));
  m_groupOnStopCombo = new QComboBox(stepsPage);
  m_groupOnStopCombo->setToolTip(
      tr("Qual grupo roda ao encerrar a gravação. Vazio = automático."));
  m_groupOnHotkeyCombo = new QComboBox(stepsPage);
  m_groupOnHotkeyCombo->setToolTip(
      tr("Qual grupo roda no atalho ou em \"Testar agora\". Vazio = automático."));
  slotForm->addRow(tr("Ao iniciar"), m_groupOnStartCombo);
  slotForm->addRow(tr("Ao encerrar"), m_groupOnStopCombo);
  slotForm->addRow(tr("No atalho"), m_groupOnHotkeyCombo);
  stepsLayout->addLayout(slotForm);

  // 3) Ações de teste / encerramento
  auto *runRow = new QHBoxLayout();
  runRow->setSpacing(4);
  m_runBtn = new QPushButton(tr("Testar agora"), stepsPage);
  m_runBtn->setToolTip(
      tr("Executa agora os passos da suíte selecionada, sem precisar gravar."));
  m_stopWithOutroBtn =
      new QPushButton(tr("Encerrar gravação com transição"), stepsPage);
  m_stopWithOutroBtn->setToolTip(
      tr("Roda o grupo de encerramento, espera as transições terminarem e só "
         "então encerra a gravação, para o esmaecer entrar no arquivo. O botão "
         "de parar do próprio OBS fecha o arquivo na hora e não dá tempo para "
         "isso."));
  runRow->addWidget(m_runBtn, 1);
  runRow->addWidget(m_stopWithOutroBtn, 1);
  stepsLayout->addLayout(runRow);

  // 4) Status de execução
  m_statusLabel = new QLabel(stepsPage);
  m_statusLabel->setWordWrap(true);
  m_statusLabel->setEnabled(false);
  m_statusLabel->setText(tr("Pronto."));
  stepsLayout->addWidget(m_statusLabel);

  // 5) Cabeçalho + lista de passos
  m_stepsHeader = new QLabel(stepsPage);
  QFont headerFont = m_stepsHeader->font();
  headerFont.setBold(true);
  m_stepsHeader->setFont(headerFont);
  m_stepsHeader->setWordWrap(true);
  m_stepsHeader->setToolTip(
      tr("Os passos abaixo pertencem a esta suíte e rodam de cima para "
         "baixo."));
  stepsLayout->addWidget(m_stepsHeader);

  m_stepList = new QListWidget(stepsPage);
  m_stepList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_stepList->setContextMenuPolicy(Qt::CustomContextMenu);
  m_stepList->setAlternatingRowColors(true);
  m_stepList->setWordWrap(true);
  m_stepList->setTextElideMode(Qt::ElideNone);
  m_stepList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_stepList->setUniformItemSizes(false);
  m_stepList->setSpacing(2);
  m_stepList->setMinimumHeight(140);
  m_stepList->setToolTip(
      tr("Clique duas vezes em um passo para editá-lo. Marque vários com "
         "Shift e use o botão direito → Mesclar para virarem um grupo. "
         "Editar, remover, mesclar e desmesclar ficam no menu de contexto."));
  m_stepList->viewport()->installEventFilter(this);
  stepsLayout->addWidget(m_stepList, 1);

  // 6) Só Adicionar / subir / descer (resto no menu de contexto)
  auto *stepButtons = new QHBoxLayout();
  stepButtons->setSpacing(4);
  m_addStepBtn = new QPushButton(tr("Adicionar"), stepsPage);
  m_addStepBtn->setToolTip(tr("Adiciona um passo no fim da lista."));
  m_upBtn = new QPushButton(tr("↑"), stepsPage);
  m_upBtn->setToolTip(tr("Move o passo selecionado uma posição para cima."));
  m_downBtn = new QPushButton(tr("↓"), stepsPage);
  m_downBtn->setToolTip(tr("Move o passo selecionado uma posição para baixo."));
  stepButtons->addWidget(m_addStepBtn);
  stepButtons->addWidget(m_upBtn);
  stepButtons->addWidget(m_downBtn);
  stepButtons->addStretch(1);
  stepsLayout->addLayout(stepButtons);
  pages->addTab(stepsPage, tr("Passos"));

  // ---- Aba Suíte: ativação e organização -------------------------------
  auto *suitePage = new QWidget(pages);
  auto *suiteLayout = new QVBoxLayout(suitePage);
  suiteLayout->setContentsMargins(6, 6, 6, 6);
  suiteLayout->setSpacing(4);

  m_activeLabel = new QLabel(suitePage);
  m_activeLabel->setWordWrap(true);
  m_activeLabel->setEnabled(false);
  suiteLayout->addWidget(m_activeLabel);

  auto *manageButtons = new QGridLayout();
  manageButtons->setSpacing(4);
  m_activateBtn = new QPushButton(tr("Ativar"), suitePage);
  m_activateBtn->setToolTip(
      tr("Torna a suíte escolhida a única ativa: é ela que roda ao gravar."));
  m_deactivateBtn = new QPushButton(tr("Desativar"), suitePage);
  m_deactivateBtn->setToolTip(tr("Nenhuma suíte roda ao iniciar a gravação."));
  m_renameBtn = new QPushButton(tr("Renomear"), suitePage);
  m_renameBtn->setToolTip(tr("Muda o nome da suíte escolhida."));
  m_duplicateBtn = new QPushButton(tr("Duplicar"), suitePage);
  m_duplicateBtn->setToolTip(
      tr("Cria uma cópia da suíte escolhida, com todos os passos."));
  m_removeBtn = new QPushButton(tr("Excluir"), suitePage);
  m_removeBtn->setToolTip(tr("Apaga a suíte escolhida."));
  m_mergeBtn = new QPushButton(tr("Mesclar..."), suitePage);
  m_mergeBtn->setToolTip(
      tr("Junta os passos de várias suítes em uma suíte nova."));
  manageButtons->addWidget(m_activateBtn, 0, 0);
  manageButtons->addWidget(m_deactivateBtn, 0, 1);
  manageButtons->addWidget(m_renameBtn, 0, 2);
  manageButtons->addWidget(m_duplicateBtn, 1, 0);
  manageButtons->addWidget(m_removeBtn, 1, 1);
  manageButtons->addWidget(m_mergeBtn, 1, 2);
  suiteLayout->addLayout(manageButtons);

  m_outroKeyBtn = new QPushButton(tr("Atalho de parada..."), suitePage);
  m_outroKeyBtn->setToolTip(
      tr("Passa a sua tecla de parar gravação do OBS para o encerramento "
         "suave, para você continuar usando a mesma tecla de sempre."));
  suiteLayout->addWidget(m_outroKeyBtn);
  suiteLayout->addStretch(1);
  pages->addTab(suitePage, tr("Suíte"));

  root->addWidget(pages, 1);

  connect(m_store, &SuiteStore::changed, this, &SuiteDock::refresh);
  connect(m_suiteTabs, &QTabBar::currentChanged, this,
          &SuiteDock::onSelectionChanged);
  connect(m_suiteTabs, &QTabBar::tabBarDoubleClicked, this,
          [this](int index) {
            if (index < 0)
              return;
            m_suiteTabs->setCurrentIndex(index);
            onActivateSelected();
          });
  connect(addBtn, &QPushButton::clicked, this, &SuiteDock::onAddSuite);
  connect(m_exampleSuiteBtn, &QPushButton::clicked, this,
          &SuiteDock::onAddExampleSuite);
  connect(m_renameBtn, &QPushButton::clicked, this, &SuiteDock::onRenameSuite);
  connect(m_duplicateBtn, &QPushButton::clicked, this,
          &SuiteDock::onDuplicateSuite);
  connect(m_removeBtn, &QPushButton::clicked, this, &SuiteDock::onRemoveSuite);
  connect(m_mergeBtn, &QPushButton::clicked, this, &SuiteDock::onMergeSuites);
  connect(m_suiteTabs, &QTabBar::customContextMenuRequested, this,
          &SuiteDock::onSuiteContextMenu);
  connect(m_activateBtn, &QPushButton::clicked, this,
          &SuiteDock::onActivateSelected);
  connect(m_deactivateBtn, &QPushButton::clicked, this,
          &SuiteDock::onClearActive);
  connect(m_openFolderCheck, &QCheckBox::toggled, this,
          &SuiteDock::onOpenFolderToggled);
  connect(m_runOnStartCheck, &QCheckBox::toggled, this,
          &SuiteDock::onRunOnStartToggled);
  connect(m_runOnStopCheck, &QCheckBox::toggled, this,
          &SuiteDock::onRunOnStopToggled);
  connect(m_groupOnStartCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SuiteDock::onGroupOnStartChanged);
  connect(m_groupOnStopCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &SuiteDock::onGroupOnStopChanged);
  connect(m_groupOnHotkeyCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SuiteDock::onGroupOnHotkeyChanged);
  connect(m_addStepBtn, &QPushButton::clicked, this, &SuiteDock::onAddStep);
  connect(m_stepList, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { onEditStep(); });
  connect(m_stepList, &QListWidget::itemSelectionChanged, this,
          &SuiteDock::updateButtonStates);
  connect(m_upBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepUp);
  connect(m_downBtn, &QPushButton::clicked, this, &SuiteDock::onMoveStepDown);
  connect(m_stepList, &QListWidget::customContextMenuRequested, this,
          &SuiteDock::onStepContextMenu);
  connect(m_runBtn, &QPushButton::clicked, this, &SuiteDock::onRunNow);
  connect(m_stopWithOutroBtn, &QPushButton::clicked, this,
          &SuiteDock::onStopWithOutro);
  connect(m_outroKeyBtn, &QPushButton::clicked, this,
          &SuiteDock::onOutroHotkeySetup);

  if (m_engine) {
    connect(m_engine, &SuiteEngine::started, this, &SuiteDock::onEngineStarted);
    connect(m_engine, &SuiteEngine::stepStarted, this,
            &SuiteDock::onEngineStepStarted);
    connect(m_engine, &SuiteEngine::stepFailed, this,
            &SuiteDock::onEngineStepFailed);
    connect(m_engine, &SuiteEngine::finished, this,
            &SuiteDock::onEngineFinished);
    connect(m_engine, &SuiteEngine::outroState, this,
            &SuiteDock::onEngineOutroState);
  }

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

void SuiteDock::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
}

QString SuiteDock::selectedSuiteId() const
{
  const int index = m_suiteTabs->currentIndex();
  if (index < 0)
    return {};
  return m_suiteTabs->tabData(index).toString();
}

QStringList SuiteDock::selectedSuiteIds() const
{
  // A escolha de várias suítes acontece no diálogo de mesclagem; aqui só
  // existe a aba atual.
  const QString id = selectedSuiteId();
  return id.isEmpty() ? QStringList() : QStringList{id};
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
  for (int i = 0; i < m_suiteTabs->count(); ++i) {
    if (m_suiteTabs->tabData(i).toString() == id) {
      m_suiteTabs->setCurrentIndex(i);
      return;
    }
  }
}

void SuiteDock::setStatusText(const QString &text)
{
  if (m_statusLabel)
    m_statusLabel->setText(text);
}

void SuiteDock::highlightRunningStep(int stepIndex)
{
  m_runningStepIndex = stepIndex;
  if (!m_stepList)
    return;

  const QColor highlight =
      palette().color(QPalette::Highlight).lighter(170);
  for (int i = 0; i < m_stepList->count(); ++i) {
    QListWidgetItem *item = m_stepList->item(i);
    const QVariant value = item->data(kRoleStepIndex);
    const bool match =
        value.isValid() && value.toInt() == stepIndex && stepIndex >= 0;
    item->setBackground(match ? QBrush(highlight) : QBrush());
    if (match) {
      m_stepList->scrollToItem(item);
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
  m_runOnStartCheck->setEnabled(hasSuite);
  m_runOnStopCheck->setEnabled(hasSuite);
  m_groupOnStartCombo->setEnabled(hasSuite);
  m_groupOnStopCombo->setEnabled(hasSuite);
  m_groupOnHotkeyCombo->setEnabled(hasSuite);

  const int stepRow = m_stepList->currentRow();
  const int stepIndex = currentStepIndex();
  const bool onHeader = stepIndex < 0 && !currentRowGroupId().isEmpty();
  const bool hasRow = hasSuite && stepRow >= 0 && (stepIndex >= 0 || onHeader);

  m_addStepBtn->setEnabled(hasSuite);
  m_upBtn->setEnabled(hasRow && stepRow > 0);
  m_downBtn->setEnabled(hasRow && stepRow < m_stepList->count() - 1);
  m_runBtn->setEnabled(hasSuite || hasActive);
  m_stopWithOutroBtn->setEnabled(hasActive);
}

void SuiteDock::refresh()
{
  if (!m_store)
    return;

  m_updating = true;
  const QString previousCurrent = selectedSuiteId();
  const int previousStepIndex = currentStepIndex();

  const QSignalBlocker blockTabs(m_suiteTabs);
  while (m_suiteTabs->count() > 0)
    m_suiteTabs->removeTab(0);

  int currentIndex = -1;
  for (const Suite &suite : m_store->suites()) {
    const bool active = suite.id == m_store->activeSuiteId();
    // A suíte ativa ganha um marcador, para a aba dizer quem roda ao gravar.
    const int index =
        m_suiteTabs->addTab(active ? tr("%1  ●").arg(suite.name) : suite.name);
    m_suiteTabs->setTabData(index, suite.id);
    m_suiteTabs->setTabToolTip(
        index, active ? tr("%1 — %2. Esta é a suíte ativa: é ela que roda ao "
                           "iniciar a gravação.")
                            .arg(suite.name,
                                 stepCountText(suite.steps.size()))
                      : tr("%1 — %2").arg(suite.name,
                                          stepCountText(suite.steps.size())));
    if (suite.id == previousCurrent)
      currentIndex = index;
  }

  if (currentIndex < 0 && m_suiteTabs->count() > 0)
    currentIndex = 0;
  if (currentIndex >= 0)
    m_suiteTabs->setCurrentIndex(currentIndex);

  const bool hasSuites = !m_store->suites().isEmpty();
  if (m_emptyState)
    m_emptyState->setVisible(!hasSuites);
  if (m_pages)
    m_pages->setVisible(hasSuites);

  const Suite *active = m_store->activeSuite();
  m_activeLabel->setText(
      active ? tr("Suíte ativa: %1 — é ela que roda ao gravar.")
                   .arg(active->name)
             : tr("Nenhuma suíte ativa: nada roda ao iniciar a gravação."));

  m_updating = false;
  onSelectionChanged();

  if (previousStepIndex >= 0)
    selectStepRowByIndex(previousStepIndex);
  updateButtonStates();
}

void SuiteDock::refreshGroupSlotCombos()
{
  auto fillCombo = [this](QComboBox *combo, const QString &currentId) {
    if (!combo)
      return;
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(tr("(automático — pelo momento do grupo)"), QString());

    Suite *suite = selectedSuite();
    if (!suite)
      return;

    QStringList added;
    for (const SuiteStep &step : suite->steps) {
      if (step.groupId.isEmpty() || added.contains(step.groupId))
        continue;
      added.append(step.groupId);
      const QString name = step.groupName.trimmed().isEmpty()
                               ? tr("Grupo sem nome")
                               : step.groupName.trimmed();
      combo->addItem(name, step.groupId);
    }

    if (!currentId.isEmpty() && !added.contains(currentId)) {
      combo->addItem(tr("Grupo removido"), currentId);
    }

    int index = combo->findData(currentId);
    if (index < 0)
      index = 0;
    combo->setCurrentIndex(index);
  };

  Suite *suite = selectedSuite();
  fillCombo(m_groupOnStartCombo, suite ? suite->groupOnStartId : QString());
  fillCombo(m_groupOnStopCombo, suite ? suite->groupOnStopId : QString());
  fillCombo(m_groupOnHotkeyCombo, suite ? suite->groupOnHotkeyId : QString());
}

void SuiteDock::applyGroupSlot(QComboBox *combo, QString *slotField,
                               SuiteGroupWhen syncWhen, bool enableRunFlag)
{
  if (m_updating || !combo || !slotField)
    return;

  Suite *suite = selectedSuite();
  if (!suite)
    return;

  const QString id = combo->currentData().toString();
  *slotField = id;

  if (!id.isEmpty()) {
    if (enableRunFlag) {
      if (syncWhen == SuiteGroupWhen::RecordingStarted)
        suite->runOnRecordingStart = true;
      else if (syncWhen == SuiteGroupWhen::RecordingStopped)
        suite->runOnRecordingStop = true;
    }
    for (SuiteStep &step : suite->steps) {
      if (step.groupId == id)
        step.groupWhen = syncWhen;
    }
  }

  persistSelectedSuite();
}

void SuiteDock::clearSuiteGroupSlots(Suite *suite, const QString &groupId)
{
  if (!suite || groupId.isEmpty())
    return;
  if (suite->groupOnStartId == groupId)
    suite->groupOnStartId.clear();
  if (suite->groupOnStopId == groupId)
    suite->groupOnStopId.clear();
  if (suite->groupOnHotkeyId == groupId)
    suite->groupOnHotkeyId.clear();
}

void SuiteDock::onSelectionChanged()
{
  if (m_updating)
    return;

  Suite *suite = selectedSuite();
  m_stepList->clear();

  // O cabeçalho deixa explícito de qual suíte são os passos mostrados.
  if (suite)
    m_stepsHeader->setText(
        suite->id == m_store->activeSuiteId()
            ? tr("%1 · %2 · suíte ativa")
                  .arg(suite->name, stepCountText(suite->steps.size()))
            : tr("%1 · %2").arg(suite->name,
                                stepCountText(suite->steps.size())));
  else
    m_stepsHeader->setText(
        tr("Nenhuma suíte ainda. Use \"+ Nova suíte\" ou o exemplo pronto."));

  const QSignalBlocker blockFolder(m_openFolderCheck);
  const QSignalBlocker blockStart(m_runOnStartCheck);
  const QSignalBlocker blockStop(m_runOnStopCheck);
  if (!suite) {
    m_openFolderCheck->setChecked(false);
    m_runOnStartCheck->setChecked(false);
    m_runOnStopCheck->setChecked(false);
    refreshGroupSlotCombos();
    updateButtonStates();
    return;
  }

  m_openFolderCheck->setChecked(suite->openRecordingFolderOnStop);
  m_runOnStartCheck->setChecked(suite->runOnRecordingStart);
  m_runOnStopCheck->setChecked(suite->runOnRecordingStop);
  refreshGroupSlotCombos();

  const QVector<SuiteStep> &steps = suite->steps;
  int block = 1;
  int i = 0;
  while (i < steps.size()) {
    const QString groupId = steps.at(i).groupId;

    if (groupId.isEmpty()) {
      const SuiteStep &step = steps.at(i);
      auto *item = new QListWidgetItem(
          tr("%1. %2 %3")
              .arg(block)
              .arg(stepTypeIcon(step.type), step.summary()),
          m_stepList);
      item->setData(kRoleStepIndex, i);
      item->setToolTip(tr("%1 — %2").arg(stepTypeLabel(step.type),
                                         stepTypeHelp(step.type)));
      QFont font = item->font();
      font.setPointSizeF(font.pointSizeF() * 0.95);
      if (step.isCondition()) {
        font.setItalic(true);
        item->setData(Qt::ForegroundRole,
                      palette().color(QPalette::Disabled, QPalette::WindowText));
      }
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
    const SuiteGroupWhen when = steps.at(i).groupWhen;
    QString headerText = tr("%1. [%2] %3 · %4 ações")
                             .arg(block)
                             .arg(groupWhenBadge(when), name)
                             .arg(members);
    if (steps.at(i).groupFade == SuiteGroupFade::FromBlack)
      headerText += tr(" · clareia do preto em %1 ms")
                        .arg(steps.at(i).groupFadeMs);
    else if (steps.at(i).groupFade == SuiteGroupFade::ToBlack)
      headerText +=
          tr(" · escurece até o preto em %1 ms").arg(steps.at(i).groupFadeMs);
    auto *header = new QListWidgetItem(headerText, m_stepList);
    header->setData(kRoleStepIndex, -1);
    header->setData(kRoleGroupId, groupId);
    QString headerHelp =
        tr("Clique duas vezes (ou use o botão direito) para escolher o nome e "
           "quando este grupo roda: ao iniciar a gravação, ao encerrar, ou "
           "sempre.");
    if (steps.at(i).groupFade == SuiteGroupFade::FromBlack)
      headerHelp += tr("\n\nA tela começa preta e clareia em %1 ms depois que "
                       "as ações do grupo terminam.")
                        .arg(steps.at(i).groupFadeMs);
    else if (steps.at(i).groupFade == SuiteGroupFade::ToBlack)
      headerHelp += tr("\n\nA tela escurece até o preto em %1 ms antes das "
                       "ações do grupo.")
                        .arg(steps.at(i).groupFadeMs);
    header->setToolTip(headerHelp);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);

    for (int k = i; k < end; ++k) {
      const SuiteStep &step = steps.at(k);
      auto *item = new QListWidgetItem(
          tr("     • %1 %2").arg(stepTypeIcon(step.type), step.summary()),
          m_stepList);
      item->setData(kRoleStepIndex, k);
      item->setData(kRoleGroupId, groupId);
      item->setToolTip(tr("%1 — %2").arg(stepTypeLabel(step.type),
                                         stepTypeHelp(step.type)));
      QFont font = item->font();
      font.setPointSizeF(font.pointSizeF() * 0.95);
      if (step.isCondition()) {
        font.setItalic(true);
        item->setData(Qt::ForegroundRole,
                      palette().color(QPalette::Disabled, QPalette::WindowText));
      }
      item->setFont(font);
    }

    i = end;
    ++block;
  }

  if (steps.isEmpty()) {
    auto *item = new QListWidgetItem(
        tr("Nenhum passo nesta suíte. Use \"Adicionar\" para criar a "
           "automação."),
        m_stepList);
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
  // A suite acabou de nascer vazia: abre logo a aba onde se montam os passos.
  if (m_pages)
    m_pages->setCurrentIndex(0);
}

void SuiteDock::onAddExampleSuite()
{
  if (!m_store)
    return;

  // createSuite ativa a primeira suíte sozinha; se já havia uma ativa, mantém.
  const bool hadActive = !m_store->activeSuiteId().isEmpty();
  Suite suite = m_store->createSuite(tr("Gravação padrão"));
  if (!hadActive)
    m_store->clearActiveSuite();

  const QString introId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString outroId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  SuiteStep introDelay;
  introDelay.type = SuiteStepType::DelayMs;
  introDelay.ms = 500;
  introDelay.groupId = introId;
  introDelay.groupName = tr("Intro");
  introDelay.groupWhen = SuiteGroupWhen::RecordingStarted;
  introDelay.groupFade = SuiteGroupFade::FromBlack;
  introDelay.groupFadeMs = 2000;

  SuiteStep outroDelay;
  outroDelay.type = SuiteStepType::DelayMs;
  outroDelay.ms = 500;
  outroDelay.groupId = outroId;
  outroDelay.groupName = tr("Outro");
  outroDelay.groupWhen = SuiteGroupWhen::RecordingStopped;
  outroDelay.groupFade = SuiteGroupFade::ToBlack;
  outroDelay.groupFadeMs = 2000;

  suite.steps = {introDelay, outroDelay};
  suite.groupOnStartId = introId;
  suite.groupOnStopId = outroId;
  suite.runOnRecordingStart = true;
  suite.runOnRecordingStop = true;
  suite.openRecordingFolderOnStop = true;

  m_store->updateSuite(suite);
  refresh();
  selectSuiteById(suite.id);
  if (m_pages)
    m_pages->setCurrentIndex(0);
  setStatusText(tr("Ative a suíte na aba Suíte"));
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
  // O menu age sobre a aba clicada, e não sobre a que estava aberta.
  const int clicked = m_suiteTabs->tabAt(pos);
  if (clicked >= 0)
    m_suiteTabs->setCurrentIndex(clicked);

  QMenu menu(this);

  QAction *create = menu.addAction(tr("Nova suíte..."));
  connect(create, &QAction::triggered, this, &SuiteDock::onAddSuite);

  QAction *example = menu.addAction(tr("Suíte de exemplo (intro + outro)..."));
  connect(example, &QAction::triggered, this, &SuiteDock::onAddExampleSuite);

  QAction *activate = menu.addAction(tr("Ativar esta suíte"));
  activate->setEnabled(!selectedSuiteId().isEmpty());
  connect(activate, &QAction::triggered, this, &SuiteDock::onActivateSelected);

  QAction *merge = menu.addAction(tr("Mesclar suítes..."));
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

  menu.exec(m_suiteTabs->mapToGlobal(pos));
}

void SuiteDock::onOpenFolderToggled(bool checked)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  suite->openRecordingFolderOnStop = checked;
  persistSelectedSuite();
}

void SuiteDock::onRunOnStartToggled(bool checked)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  suite->runOnRecordingStart = checked;
  persistSelectedSuite();
}

void SuiteDock::onRunOnStopToggled(bool checked)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  suite->runOnRecordingStop = checked;
  persistSelectedSuite();
}

void SuiteDock::onGroupOnStartChanged(int)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  applyGroupSlot(m_groupOnStartCombo, &suite->groupOnStartId,
                 SuiteGroupWhen::RecordingStarted, true);
}

void SuiteDock::onGroupOnStopChanged(int)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  applyGroupSlot(m_groupOnStopCombo, &suite->groupOnStopId,
                 SuiteGroupWhen::RecordingStopped, true);
}

void SuiteDock::onGroupOnHotkeyChanged(int)
{
  Suite *suite = selectedSuite();
  if (!suite)
    return;
  applyGroupSlot(m_groupOnHotkeyCombo, &suite->groupOnHotkeyId,
                 SuiteGroupWhen::Manual, false);
}

bool SuiteDock::editStepDialog(SuiteStep &step, bool isNew)
{
  QDialog dialog(this);
  dialog.setWindowTitle(isNew ? tr("Adicionar passo") : tr("Editar passo"));
  dialog.setMinimumWidth(460);

  auto *root = new QVBoxLayout(&dialog);
  root->setSpacing(10);

  auto *typeForm = new QFormLayout();
  auto *categoryCombo = new QComboBox(&dialog);
  categoryCombo->addItem(categoryLabel(StepCategory::Scene),
                         static_cast<int>(StepCategory::Scene));
  categoryCombo->addItem(categoryLabel(StepCategory::Source),
                         static_cast<int>(StepCategory::Source));
  categoryCombo->addItem(categoryLabel(StepCategory::Audio),
                         static_cast<int>(StepCategory::Audio));
  categoryCombo->addItem(categoryLabel(StepCategory::Screen),
                         static_cast<int>(StepCategory::Screen));
  categoryCombo->addItem(categoryLabel(StepCategory::Wait),
                         static_cast<int>(StepCategory::Wait));
  categoryCombo->addItem(categoryLabel(StepCategory::Condition),
                         static_cast<int>(StepCategory::Condition));
  categoryCombo->addItem(categoryLabel(StepCategory::Open),
                         static_cast<int>(StepCategory::Open));
  typeForm->addRow(new QLabel(tr("Categoria:"), &dialog), categoryCombo);

  auto *typeCombo = new QComboBox(&dialog);
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

  auto *triggerCombo = new QComboBox(&dialog);
  triggerCombo->addItem(tr("A gravação começou"),
                        static_cast<int>(SuiteTrigger::RecordingStarted));
  triggerCombo->addItem(tr("A gravação terminou"),
                        static_cast<int>(SuiteTrigger::RecordingStopped));
  triggerCombo->addItem(tr("O atalho ou \"Testar agora\" foi usado"),
                        static_cast<int>(SuiteTrigger::Manual));
  auto *triggerLabel = addRow(tr("Só continuar quando:"), triggerCombo);

  // Alvo da condição: o bloco seguinte ou um grupo mesclado desta suíte.
  auto *targetCombo = new QComboBox(&dialog);
  targetCombo->addItem(tr("O passo ou grupo logo abaixo"), QString());
  {
    QStringList added;
    if (const Suite *current = selectedSuite()) {
      for (const SuiteStep &other : current->steps) {
        if (other.groupId.isEmpty() || added.contains(other.groupId))
          continue;
        added.append(other.groupId);
        const QString name = other.groupName.trimmed().isEmpty()
                                 ? tr("Grupo sem nome")
                                 : other.groupName.trimmed();
        targetCombo->addItem(tr("Grupo: %1").arg(name), other.groupId);
      }
    }
    // Um grupo apagado depois nao pode sumir da condicao sem avisar.
    if (!step.conditionGroupId.isEmpty() &&
        !added.contains(step.conditionGroupId)) {
      targetCombo->addItem(tr("Grupo removido: %1").arg(step.conditionGroupName),
                           step.conditionGroupId);
    }
  }
  auto *targetLabel = addRow(tr("Aplicar a condição a:"), targetCombo);

  auto *muteCombo = new QComboBox(&dialog);
  muteCombo->addItem(tr("Silenciar (deixar no mudo)"), true);
  muteCombo->addItem(tr("Reativar o som (tirar do mudo)"), false);
  auto *muteLabel = addRow(tr("Ação no áudio:"), muteCombo);

  auto *screenDirCombo = new QComboBox(&dialog);
  screenDirCombo->addItem(tr("Clarear — começar no preto e revelar a imagem"),
                          true);
  screenDirCombo->addItem(tr("Escurecer — levar a imagem até o preto total"),
                          false);
  auto *screenDirLabel = addRow(tr("O que fazer com a tela:"), screenDirCombo);

  auto *screenSecSpin = new QDoubleSpinBox(&dialog);
  screenSecSpin->setRange(0.1, 60.0);
  screenSecSpin->setDecimals(1);
  screenSecSpin->setSingleStep(0.5);
  screenSecSpin->setSuffix(tr(" s"));
  screenSecSpin->setToolTip(
      tr("2,0 s são dois segundos reais de esmaecer. O passo espera este "
         "tempo terminar antes de seguir."));
  auto *screenSecLabel = addRow(tr("Duração:"), screenSecSpin);

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

  auto *fadeSecSpin = new QDoubleSpinBox(&dialog);
  fadeSecSpin->setRange(0.1, 60.0);
  fadeSecSpin->setDecimals(1);
  fadeSecSpin->setSingleStep(0.1);
  fadeSecSpin->setSuffix(tr(" s"));
  auto *fadeSecLabel = addRow(tr("Duração do fade:"), fadeSecSpin);

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

  auto *delaySecSpin = new QDoubleSpinBox(&dialog);
  delaySecSpin->setRange(0.1, 60.0);
  delaySecSpin->setDecimals(1);
  delaySecSpin->setSingleStep(0.1);
  delaySecSpin->setSuffix(tr(" s"));
  auto *delayLabel = addRow(tr("Esperar:"), delaySecSpin);

  auto *urlEdit = new QLineEdit(&dialog);
  urlEdit->setPlaceholderText(tr("https://exemplo.com ou D:\\Videos"));
  auto *urlLabel = addRow(tr("Site, pasta ou programa:"), urlEdit);

  // Opções avançadas: recolhidas por padrão.
  auto *advancedBox = new QGroupBox(tr("Opções avançadas"), &dialog);
  advancedBox->setCheckable(true);
  advancedBox->setChecked(false);
  advancedBox->setFlat(true);
  auto *advancedForm = new QFormLayout(advancedBox);
  advancedForm->setSpacing(8);

  auto *itemTransitionCombo = new QComboBox(advancedBox);
  itemTransitionCombo->addItem(tr("Não mexer (usar o que já está na fonte)"),
                               QString());
  itemTransitionCombo->addItem(tr("Sem transição (aparece ou sai na hora)"),
                               QStringLiteral("none"));
  for (const auto &type : SuiteActions::itemTransitionTypes())
    itemTransitionCombo->addItem(type.second, type.first);
  auto *itemTransitionLabel =
      new QLabel(tr("Transição da fonte:"), advancedBox);
  advancedForm->addRow(itemTransitionLabel, itemTransitionCombo);

  auto *itemTransitionSpin = new QSpinBox(advancedBox);
  itemTransitionSpin->setRange(0, 60000);
  itemTransitionSpin->setSuffix(QStringLiteral(" ms"));
  itemTransitionSpin->setSingleStep(50);
  auto *itemTransitionSpinLabel =
      new QLabel(tr("Velocidade da transição da fonte:"), advancedBox);
  advancedForm->addRow(itemTransitionSpinLabel, itemTransitionSpin);

  auto *waitFadeCheck = new QCheckBox(
      tr("Esperar o fade terminar antes do próximo passo"), advancedBox);
  waitFadeCheck->setToolTip(
      tr("Desmarque para o fade continuar em segundo plano enquanto os passos "
         "seguintes rodam."));
  advancedForm->addRow(waitFadeCheck);
  root->addWidget(advancedBox);

  auto setRowVisible = [](QLabel *label, QWidget *field, bool visible) {
    label->setVisible(visible);
    field->setVisible(visible);
  };

  auto fillTypesForCategory = [&](StepCategory category,
                                  SuiteStepType preferType) {
    const QSignalBlocker blocker(typeCombo);
    typeCombo->clear();
    const auto addType = [&](SuiteStepType type) {
      typeCombo->addItem(stepTypeLabel(type), static_cast<int>(type));
    };
    switch (category) {
    case StepCategory::Scene:
      addType(SuiteStepType::SetScene);
      addType(SuiteStepType::SetTransition);
      break;
    case StepCategory::Source:
      addType(SuiteStepType::SetSourceVisible);
      addType(SuiteStepType::RestartMedia);
      break;
    case StepCategory::Audio:
      addType(SuiteStepType::SetMute);
      addType(SuiteStepType::SetVolume);
      addType(SuiteStepType::AudioFade);
      break;
    case StepCategory::Screen:
      addType(SuiteStepType::ScreenFade);
      break;
    case StepCategory::Wait:
      addType(SuiteStepType::DelayMs);
      break;
    case StepCategory::Condition:
      addType(SuiteStepType::IfCurrentScene);
      addType(SuiteStepType::IfSourceVisible);
      addType(SuiteStepType::IfTrigger);
      break;
    case StepCategory::Open:
      addType(SuiteStepType::OpenUrl);
      break;
    }
    int index = 0;
    for (int i = 0; i < typeCombo->count(); ++i) {
      if (static_cast<SuiteStepType>(typeCombo->itemData(i).toInt()) ==
          preferType) {
        index = i;
        break;
      }
    }
    typeCombo->setCurrentIndex(index);
  };

  auto fillSources = [&](SuiteStepType type) {
    const QString currentName =
        sourceCombo->currentData().isValid()
            ? sourceCombo->currentData().toString()
            : sourceCombo->currentText();
    sourceCombo->blockSignals(true);
    sourceCombo->clear();

    auto addEntry = [&](const QString &label, const QString &name) {
      sourceCombo->addItem(label, name);
    };
    auto addSceneEntries = [&]() {
      for (const auto &entry :
           SuiteActions::sourceEntriesInScene(sceneCombo->currentText())) {
        addEntry(entry.first, entry.second);
      }
    };

    if (type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::AudioFade) {
      QStringList seen;
      for (const QString &name : SuiteActions::audioSourceNames()) {
        addEntry(name, name);
        seen.append(name);
      }
      for (const auto &entry :
           SuiteActions::sourceEntriesInScene(sceneCombo->currentText())) {
        if (seen.contains(entry.second))
          continue;
        addEntry(entry.first, entry.second);
        seen.append(entry.second);
      }
    } else if (type == SuiteStepType::RestartMedia) {
      addSceneEntries();
      if (sourceCombo->count() == 0) {
        for (const QString &name : SuiteActions::audioSourceNames())
          addEntry(name, name);
      }
    } else {
      addSceneEntries();
    }

    int index = sourceCombo->findData(currentName);
    if (index < 0 && !currentName.isEmpty()) {
      sourceCombo->addItem(currentName, currentName);
      index = sourceCombo->count() - 1;
    }
    if (index >= 0)
      sourceCombo->setCurrentIndex(index);
    sourceCombo->blockSignals(false);
  };

  auto applyType = [&]() {
    if (typeCombo->count() == 0)
      return;
    const auto type =
        static_cast<SuiteStepType>(typeCombo->currentData().toInt());

    const bool needScene = type == SuiteStepType::SetScene ||
                           type == SuiteStepType::SetSourceVisible ||
                           type == SuiteStepType::IfCurrentScene ||
                           type == SuiteStepType::IfSourceVisible ||
                           type == SuiteStepType::ScreenFade;
    const bool needSource = type == SuiteStepType::SetSourceVisible ||
                            type == SuiteStepType::IfSourceVisible ||
                            type == SuiteStepType::SetMute ||
                            type == SuiteStepType::SetVolume ||
                            type == SuiteStepType::AudioFade ||
                            type == SuiteStepType::RestartMedia;
    const bool needTransition = type == SuiteStepType::SetScene ||
                                type == SuiteStepType::SetTransition;

    const bool hasAudio =
        needSource && SuiteActions::sourceHasAudio(
                          sourceCombo->currentData().isValid()
                              ? sourceCombo->currentData().toString()
                              : sourceCombo->currentText());
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
    setRowVisible(triggerLabel, triggerCombo, type == SuiteStepType::IfTrigger);
    setRowVisible(targetLabel, targetCombo,
                  type == SuiteStepType::IfCurrentScene ||
                      type == SuiteStepType::IfSourceVisible ||
                      type == SuiteStepType::IfTrigger);
    setRowVisible(muteLabel, muteCombo, type == SuiteStepType::SetMute);
    setRowVisible(fadeDirLabel, fadeDirCombo, type == SuiteStepType::AudioFade);
    setRowVisible(screenDirLabel, screenDirCombo,
                  type == SuiteStepType::ScreenFade);
    setRowVisible(screenSecLabel, screenSecSpin,
                  type == SuiteStepType::ScreenFade);
    setRowVisible(fadeCheckLabel, fadeCheck, canOfferFade);
    setRowVisible(volumeLabel, volumeSpin, needVolume);
    setRowVisible(fadeSecLabel, fadeSecSpin, fadeOn);
    setRowVisible(transitionLabel, transitionCombo, needTransition);
    setRowVisible(transitionSpinLabel, transitionSpin, needTransition);
    setRowVisible(delayLabel, delaySecSpin, type == SuiteStepType::DelayMs);
    setRowVisible(urlLabel, urlEdit, type == SuiteStepType::OpenUrl);

    const bool showItemTransition = type == SuiteStepType::SetSourceVisible;
    itemTransitionLabel->setVisible(showItemTransition);
    itemTransitionCombo->setVisible(showItemTransition);
    const bool showItemMs =
        showItemTransition &&
        !itemTransitionCombo->currentData().toString().isEmpty() &&
        itemTransitionCombo->currentData().toString() != QLatin1String("none");
    itemTransitionSpinLabel->setVisible(showItemMs);
    itemTransitionSpin->setVisible(showItemMs);
    waitFadeCheck->setVisible(fadeOn);
    advancedBox->setVisible(showItemTransition || fadeOn);

    if (type == SuiteStepType::SetVolume)
      volumeLabel->setText(tr("Volume (0% = sem som):"));
    else
      volumeLabel->setText(tr("Volume no fim do fade:"));

    if (type == SuiteStepType::SetScene)
      sceneLabel->setText(tr("Ir para a cena:"));
    else if (type == SuiteStepType::IfCurrentScene)
      sceneLabel->setText(tr("Cena que precisa estar no ar:"));
    else if (type == SuiteStepType::ScreenFade)
      sceneLabel->setText(tr("Cena (vazio = a que estiver no ar):"));
    else
      sceneLabel->setText(tr("Cena onde está a fonte:"));

    if (type == SuiteStepType::SetMute || type == SuiteStepType::SetVolume ||
        type == SuiteStepType::AudioFade)
      sourceLabel->setText(tr("Fonte de áudio:"));
    else if (type == SuiteStepType::RestartMedia)
      sourceLabel->setText(tr("Fonte de mídia:"));
    else
      sourceLabel->setText(tr("Fonte (grupos e conteúdo interno):"));

    if (needSource)
      fillSources(type);

    dialog.adjustSize();
  };

  connect(categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) {
            const auto category = static_cast<StepCategory>(
                categoryCombo->currentData().toInt());
            const auto prefer =
                typeCombo->count() > 0
                    ? static_cast<SuiteStepType>(typeCombo->currentData().toInt())
                    : step.type;
            fillTypesForCategory(category, prefer);
            applyType();
          });
  connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(sceneCombo, &QComboBox::currentTextChanged, &dialog,
          [&](const QString &) { applyType(); });
  connect(sourceCombo, &QComboBox::currentTextChanged, &dialog,
          [&](const QString &) { applyType(); });
  connect(visibleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(itemTransitionCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
          [&](int) { applyType(); });
  connect(fadeDirCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { applyType(); });
  connect(fadeCheck, &QCheckBox::toggled, &dialog,
          [&](bool) { applyType(); });

  const StepCategory initialCategory = categoryForType(step.type);
  for (int i = 0; i < categoryCombo->count(); ++i) {
    if (static_cast<StepCategory>(categoryCombo->itemData(i).toInt()) ==
        initialCategory) {
      categoryCombo->setCurrentIndex(i);
      break;
    }
  }
  fillTypesForCategory(initialCategory, step.type);

  sceneCombo->setCurrentText(step.scene);
  transitionCombo->setCurrentText(step.transition);
  transitionSpin->setValue(step.transitionMs);
  delaySecSpin->setValue(secondsFromMs(step.ms > 0 ? step.ms : 1000));
  visibleCombo->setCurrentIndex(step.visible ? 0 : 1);
  expectCombo->setCurrentIndex(step.visible ? 0 : 1);
  muteCombo->setCurrentIndex(step.muted ? 0 : 1);
  volumeSpin->setValue(step.volume * 100.0);
  fadeDirCombo->setCurrentIndex(step.fadeIn ? 0 : 1);
  screenDirCombo->setCurrentIndex(step.fadeIn ? 0 : 1);
  screenSecSpin->setValue(
      secondsFromMs(step.screenFadeMs > 0 ? step.screenFadeMs : 2000));
  fadeCheck->setChecked(step.fadeAudio);
  fadeSecSpin->setValue(secondsFromMs(step.fadeMs > 0 ? step.fadeMs : 500));
  waitFadeCheck->setChecked(step.waitForFade);
  urlEdit->setText(step.url);
  itemTransitionCombo->setCurrentIndex(
      qMax(0, itemTransitionCombo->findData(step.itemTransitionId)));
  itemTransitionSpin->setValue(step.itemTransitionMs);
  for (int i = 0; i < triggerCombo->count(); ++i) {
    if (triggerCombo->itemData(i).toInt() == static_cast<int>(step.trigger)) {
      triggerCombo->setCurrentIndex(i);
      break;
    }
  }
  for (int i = 0; i < targetCombo->count(); ++i) {
    if (targetCombo->itemData(i).toString() == step.conditionGroupId) {
      targetCombo->setCurrentIndex(i);
      break;
    }
  }
  applyType();
  {
    const int sourceIndex = sourceCombo->findData(step.source);
    if (sourceIndex >= 0) {
      sourceCombo->setCurrentIndex(sourceIndex);
    } else if (!step.source.isEmpty()) {
      sourceCombo->addItem(step.source, step.source);
      sourceCombo->setCurrentIndex(sourceCombo->count() - 1);
    }
  }
  applyType();

  // Se o passo já tem opções avançadas preenchidas, abre o grupo.
  if (step.waitForFade == false || !step.itemTransitionId.isEmpty())
    advancedBox->setChecked(true);

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
  step.ms = msFromSeconds(delaySecSpin->value());
  step.muted = muteCombo->currentData().toBool();
  step.volume = volumeSpin->value() / 100.0;
  step.url = urlEdit->text().trimmed();
  step.scene = sceneCombo->currentText().trimmed();
  {
    const int idx = sourceCombo->currentIndex();
    if (idx >= 0 &&
        sourceCombo->itemText(idx) == sourceCombo->currentText()) {
      step.source = sourceCombo->itemData(idx).toString();
    } else {
      step.source = sourceCombo->currentText().trimmed();
    }
    if (step.source.isEmpty())
      step.source = sourceCombo->currentText().trimmed();
  }
  step.transition = transitionCombo->currentText().trimmed();
  step.visible = type == SuiteStepType::IfSourceVisible
                     ? expectCombo->currentData().toBool()
                     : visibleCombo->currentData().toBool();
  step.screenFadeMs = msFromSeconds(screenSecSpin->value());
  step.fadeIn = type == SuiteStepType::AudioFade
                    ? fadeDirCombo->currentData().toBool()
                    : (type == SuiteStepType::ScreenFade
                           ? screenDirCombo->currentData().toBool()
                           : step.visible);
  step.fadeMs = msFromSeconds(fadeSecSpin->value());
  step.waitForFade = waitFadeCheck->isChecked();
  step.fadeAudio = fadeCheck->isChecked() &&
                   SuiteActions::sourceHasAudio(step.source) &&
                   (type == SuiteStepType::SetVolume ||
                    type == SuiteStepType::SetSourceVisible);
  step.trigger = static_cast<SuiteTrigger>(triggerCombo->currentData().toInt());

  if (type == SuiteStepType::SetSourceVisible) {
    step.itemTransitionId = itemTransitionCombo->currentData().toString();
    step.itemTransitionName =
        step.itemTransitionId.isEmpty() ||
                step.itemTransitionId == QLatin1String("none")
            ? QString()
            : itemTransitionCombo->currentText();
    step.itemTransitionMs = itemTransitionSpin->value();
  } else {
    step.itemTransitionId.clear();
    step.itemTransitionName.clear();
  }

  step.conditionGroupId.clear();
  step.conditionGroupName.clear();
  if (step.isCondition()) {
    step.conditionGroupId = targetCombo->currentData().toString();
    if (!step.conditionGroupId.isEmpty()) {
      if (const Suite *current = selectedSuite()) {
        for (const SuiteStep &other : current->steps) {
          if (other.groupId == step.conditionGroupId) {
            step.conditionGroupName = other.groupName;
            break;
          }
        }
      }
    }
  }

  // Uma condicao de fim de gravacao so faz sentido se a suite rodar nessa hora.
  if (step.type == SuiteStepType::IfTrigger &&
      step.trigger == SuiteTrigger::RecordingStopped) {
    Suite *current = selectedSuite();
    if (current && !current->runOnRecordingStop) {
      current->runOnRecordingStop = true;
      QMessageBox::information(
          this, tr("Rodar ao encerrar a gravação"),
          tr("Liguei a opção \"Rodar ao encerrar\" nesta suíte, senão esta "
             "condição nunca seria verdadeira."));
    }
  }
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

void SuiteDock::releaseConditionTargets(Suite *suite, const QString &groupId)
{
  if (!suite || groupId.isEmpty())
    return;
  // O grupo deixou de existir: as condições voltam a valer para o que vem
  // logo abaixo delas, em vez de apontarem para o vazio.
  for (SuiteStep &step : suite->steps) {
    if (step.conditionGroupId != groupId)
      continue;
    step.conditionGroupId.clear();
    step.conditionGroupName.clear();
  }
  clearSuiteGroupSlots(suite, groupId);
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
    // Cabecalho de grupo: abre as opcoes do grupo.
    editGroup(currentRowGroupId());
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
    releaseConditionTargets(suite, groupId);
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

bool SuiteDock::groupOptionsDialog(SuiteGroupInfo &info, bool isNew)
{
  QDialog dialog(this);
  dialog.setWindowTitle(isNew ? tr("Mesclar passos em um grupo")
                              : tr("Opções do grupo"));
  dialog.setMinimumWidth(440);

  auto *root = new QVBoxLayout(&dialog);
  root->setSpacing(10);

  auto *form = new QFormLayout();
  form->setSpacing(8);

  auto *nameEdit = new QLineEdit(info.name, &dialog);
  nameEdit->setPlaceholderText(tr("Exemplo: Começando a gravação"));
  form->addRow(new QLabel(tr("Nome do grupo:"), &dialog), nameEdit);

  auto *whenCombo = new QComboBox(&dialog);
  for (const SuiteGroupWhen option :
       {SuiteGroupWhen::Always, SuiteGroupWhen::RecordingStarted,
        SuiteGroupWhen::RecordingStopped, SuiteGroupWhen::Manual}) {
    whenCombo->addItem(suiteGroupWhenOption(option), static_cast<int>(option));
  }
  for (int i = 0; i < whenCombo->count(); ++i) {
    if (whenCombo->itemData(i).toInt() == static_cast<int>(info.when)) {
      whenCombo->setCurrentIndex(i);
      break;
    }
  }
  form->addRow(new QLabel(tr("Este grupo roda:"), &dialog), whenCombo);

  // Transicao de tela do grupo: sair do preto no fim, ou ir ao preto antes.
  auto *fadeCombo = new QComboBox(&dialog);
  for (const SuiteGroupFade option :
       {SuiteGroupFade::None, SuiteGroupFade::FromBlack,
        SuiteGroupFade::ToBlack}) {
    fadeCombo->addItem(suiteGroupFadeOption(option), static_cast<int>(option));
  }
  fadeCombo->setCurrentIndex(
      qMax(0, fadeCombo->findData(static_cast<int>(info.fade))));
  auto *fadeLabel = new QLabel(tr("Transição de tela do grupo:"), &dialog);
  form->addRow(fadeLabel, fadeCombo);

  auto *durationSpin = new QSpinBox(&dialog);
  durationSpin->setRange(100, 60000);
  durationSpin->setSuffix(QStringLiteral(" ms"));
  durationSpin->setSingleStep(500);
  durationSpin->setValue(info.fadeMs);
  auto *durationLabel =
      new QLabel(tr("Velocidade da transição (2000 ms = 2 s):"), &dialog);
  form->addRow(durationLabel, durationSpin);
  root->addLayout(form);

  auto updateDurationRow = [&]() {
    const bool on = fadeCombo->currentData().toInt() !=
                    static_cast<int>(SuiteGroupFade::None);
    durationLabel->setVisible(on);
    durationSpin->setVisible(on);
    dialog.adjustSize();
  };
  connect(fadeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dialog, [&](int) { updateDurationRow(); });
  updateDurationRow();

  auto *hint = new QLabel(
      tr("As ações do grupo rodam em sequência, de cima para baixo. Escolhendo "
         "\"Ao iniciar a gravação\", elas acontecem quando você manda gravar; "
         "escolhendo \"Ao encerrar a gravação\", acontecem quando a gravação "
         "para.\n\nNo grupo de início use \"Clarear\": a gravação começa na "
         "tela preta, as ações acontecem escondidas e a imagem aparece aos "
         "poucos. No grupo de encerramento use \"Escurecer\": a imagem vai até "
         "o preto e só então as ações rodam, com a gravação fechando no "
         "escuro."),
      &dialog);
  hint->setWordWrap(true);
  hint->setEnabled(false);
  root->addWidget(hint);

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)
      ->setText(isNew ? tr("Mesclar") : tr("Salvar"));
  buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
  root->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return false;

  info.name = nameEdit->text().trimmed();
  info.when = static_cast<SuiteGroupWhen>(whenCombo->currentData().toInt());
  info.fade = static_cast<SuiteGroupFade>(fadeCombo->currentData().toInt());
  info.fadeMs = durationSpin->value();
  return true;
}

void SuiteDock::editGroup(const QString &groupId)
{
  Suite *suite = selectedSuite();
  if (!suite || groupId.isEmpty())
    return;

  SuiteGroupInfo info;
  bool found = false;
  for (const SuiteStep &step : suite->steps) {
    if (step.groupId != groupId)
      continue;
    info.name = step.groupName;
    info.when = step.groupWhen;
    info.fade = step.groupFade;
    info.fadeMs = step.groupFadeMs;
    found = true;
    break;
  }
  if (!found)
    return;

  if (!groupOptionsDialog(info, false))
    return;

  for (SuiteStep &step : suite->steps) {
    if (step.groupId == groupId) {
      step.groupName = info.name;
      step.groupWhen = info.when;
      step.groupFade = info.fade;
      step.groupFadeMs = info.fadeMs;
    }
    // As condições que apontam para o grupo mostram o nome novo.
    if (step.conditionGroupId == groupId)
      step.conditionGroupName = info.name;
  }
  ensureSuiteRunsFor(suite, info.when);
  persistSelectedSuite();
  refreshGroupSlotCombos();
}

void SuiteDock::setGroupWhen(const QString &groupId, SuiteGroupWhen when)
{
  Suite *suite = selectedSuite();
  if (!suite || groupId.isEmpty())
    return;
  bool touched = false;
  for (SuiteStep &step : suite->steps) {
    if (step.groupId != groupId)
      continue;
    step.groupWhen = when;
    touched = true;
  }
  if (!touched)
    return;
  ensureSuiteRunsFor(suite, when);
  persistSelectedSuite();
}

void SuiteDock::ensureSuiteRunsFor(Suite *suite, SuiteGroupWhen when)
{
  if (!suite)
    return;
  // Um grupo preso a um momento da gravação só roda se a suíte for chamada
  // naquele momento. Sem isso, o grupo nunca aconteceria.
  if (when == SuiteGroupWhen::RecordingStarted && !suite->runOnRecordingStart) {
    suite->runOnRecordingStart = true;
    QMessageBox::information(
        this, tr("Rodar ao iniciar a gravação"),
        tr("Liguei \"Rodar ao iniciar\" nesta suíte, senão este grupo nunca "
           "aconteceria."));
  }
  if (when == SuiteGroupWhen::RecordingStopped && !suite->runOnRecordingStop) {
    suite->runOnRecordingStop = true;
    QMessageBox::information(
        this, tr("Rodar ao encerrar a gravação"),
        tr("Liguei \"Rodar ao encerrar\" nesta suíte, senão este grupo nunca "
           "aconteceria."));
  }
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

  SuiteGroupInfo info;
  info.name = tr("Ações da gravação");
  if (!groupOptionsDialog(info, true))
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
    step.groupName = info.name;
    step.groupWhen = info.when;
    step.groupFade = info.fade;
    step.groupFadeMs = info.fadeMs;
  }
  ensureSuiteRunsFor(suite, info.when);

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
  refreshGroupSlotCombos();
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
    suite->steps[i].groupWhen = SuiteGroupWhen::Always;
    suite->steps[i].groupFade = SuiteGroupFade::None;
  }
  releaseConditionTargets(suite, groupId);
  persistSelectedSuite();
  if (firstIndex >= 0)
    selectStepRowByIndex(firstIndex);
  refreshGroupSlotCombos();
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

  if (onHeader) {
    const QString groupId = currentRowGroupId();
    SuiteGroupWhen currentWhen = SuiteGroupWhen::Always;
    for (const SuiteStep &step : suite->steps) {
      if (step.groupId == groupId) {
        currentWhen = step.groupWhen;
        break;
      }
    }

    QMenu *whenMenu = menu.addMenu(tr("Este grupo roda"));
    for (const SuiteGroupWhen option :
         {SuiteGroupWhen::Always, SuiteGroupWhen::RecordingStarted,
          SuiteGroupWhen::RecordingStopped, SuiteGroupWhen::Manual}) {
      QAction *action = whenMenu->addAction(suiteGroupWhenOption(option));
      action->setCheckable(true);
      action->setChecked(option == currentWhen);
      connect(action, &QAction::triggered, this,
              [this, groupId, option]() { setGroupWhen(groupId, option); });
    }
  }

  menu.addSeparator();

  QAction *edit = menu.addAction(onHeader ? tr("Opções do grupo...")
                                          : tr("Editar passo..."));
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
  SuiteTrigger trigger = SuiteTrigger::Manual;

  // Se a suíte tem grupos presos ao início ou ao fim da gravação, o teste
  // pergunta qual momento simular, senão eles não rodariam no teste.
  const Suite *suite = m_store ? (id.isEmpty() ? m_store->activeSuite()
                                               : m_store->suiteById(id))
                               : nullptr;
  bool hasStart = false;
  bool hasStop = false;
  if (suite) {
    for (const SuiteStep &step : suite->steps) {
      if (step.groupId.isEmpty())
        continue;
      if (step.groupWhen == SuiteGroupWhen::RecordingStarted)
        hasStart = true;
      else if (step.groupWhen == SuiteGroupWhen::RecordingStopped)
        hasStop = true;
    }
  }

  if (hasStart || hasStop) {
    QStringList options;
    QVector<SuiteTrigger> values;
    if (hasStart) {
      options << tr("Como se a gravação tivesse começado");
      values << SuiteTrigger::RecordingStarted;
    }
    if (hasStop) {
      options << tr("Como se a gravação tivesse terminado");
      values << SuiteTrigger::RecordingStopped;
    }
    options << tr("Como o atalho (só os grupos sem momento fixo)");
    values << SuiteTrigger::Manual;

    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("Testar agora"),
        tr("Esta suíte tem grupos com momento fixo. O que você quer simular?"),
        options, 0, false, &ok);
    if (!ok)
      return;
    const int chosen = options.indexOf(choice);
    if (chosen >= 0)
      trigger = values.at(chosen);
  }

  if (id.isEmpty())
    m_engine->runActiveNow(trigger);
  else
    m_engine->runSuiteById(id, trigger);
}

void SuiteDock::onStopWithOutro()
{
  if (!m_engine)
    return;

  if (!SuiteActions::isRecording()) {
    QMessageBox::information(
        this, tr("Encerrar gravação com transição"),
        tr("Não há gravação em andamento agora."));
    return;
  }

  const Suite *suite = m_store ? m_store->activeSuite() : nullptr;
  if (!suite || !suite->runOnRecordingStop || suite->steps.isEmpty()) {
    const auto answer = QMessageBox::question(
        this, tr("Encerrar gravação com transição"),
        tr("A suíte ativa não tem nada marcado para rodar ao encerrar a "
           "gravação, então não há transição para esperar. Encerrar a gravação "
           "agora mesmo?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes)
      return;
  }

  m_engine->stopRecordingWithOutro();
}

void SuiteDock::onEngineStarted(const QString &suiteName)
{
  m_runningStepIndex = -1;
  setStatusText(tr("Executando «%1»…").arg(suiteName));
}

void SuiteDock::onEngineStepStarted(int stepIndex, const QString &summary)
{
  setStatusText(tr("Executando: %1").arg(summary));

  // O indice da fila pode nao bater com o da suíte (fades injetados):
  // preferimos destacar pela linha cujo texto contém o resumo.
  bool found = false;
  if (m_stepList && !summary.isEmpty()) {
    const QColor highlight =
        palette().color(QPalette::Highlight).lighter(170);
    for (int i = 0; i < m_stepList->count(); ++i) {
      QListWidgetItem *item = m_stepList->item(i);
      item->setBackground(QBrush());
      if (!found && item->data(kRoleStepIndex).toInt() >= 0 &&
          item->text().contains(summary)) {
        item->setBackground(QBrush(highlight));
        m_stepList->scrollToItem(item);
        found = true;
      }
    }
  }
  if (!found)
    highlightRunningStep(stepIndex);
  else
    m_runningStepIndex = stepIndex;
}

void SuiteDock::onEngineStepFailed(const QString &summary)
{
  setStatusText(tr("Falhou: %1").arg(summary));
}

void SuiteDock::onEngineFinished()
{
  m_runningStepIndex = -1;
  if (m_stepList) {
    for (int i = 0; i < m_stepList->count(); ++i)
      m_stepList->item(i)->setBackground(QBrush());
  }
  setStatusText(tr("Concluído."));
}

void SuiteDock::onEngineOutroState(bool active)
{
  if (active)
    setStatusText(tr("Encerrando com transição…"));
}

void SuiteDock::onOutroHotkeySetup()
{
  const QString obsKey = SuiteOutroHotkey::obsStopKeyText();
  const QString ourKey = SuiteOutroHotkey::outroKeyText();

  QString state;
  if (!ourKey.isEmpty()) {
    state = tr("Agora a tecla <b>%1</b> roda o encerramento suave: ela executa "
               "os grupos marcados para o fim, espera as transições e só então "
               "encerra a gravação.")
                .arg(ourKey);
    if (!obsKey.isEmpty()) {
      state += tr("<br><br>A tecla <b>%1</b> continua no \"Parar gravação\" do "
                  "OBS, que encerra o arquivo na hora, sem esperar o "
                  "escurecer.")
                   .arg(obsKey);
    }
  } else if (!obsKey.isEmpty()) {
    state = tr("Hoje a tecla <b>%1</b> está no \"Parar gravação\" do OBS, que "
               "fecha o arquivo imediatamente — por isso o escurecer do fim "
               "não entra no vídeo.<br><br>Se você passar essa tecla para o "
               "encerramento suave, ela continua sendo a sua tecla de parar, "
               "mas o vídeo só termina depois que a transição acabar.")
                .arg(obsKey);
  } else {
    state = tr("Não há tecla no \"Parar gravação\" do OBS nem no encerramento "
               "suave. Defina uma tecla em Configurações → Atalhos.");
  }

  QMessageBox box(this);
  box.setWindowTitle(tr("Atalho para encerrar a gravação"));
  box.setTextFormat(Qt::RichText);
  box.setText(state);
  QPushButton *takeBtn = box.addButton(tr("Usar essa tecla no encerramento suave"),
                                       QMessageBox::AcceptRole);
  QPushButton *giveBtn = box.addButton(tr("Devolver a tecla ao OBS"),
                                       QMessageBox::DestructiveRole);
  box.addButton(tr("Fechar"), QMessageBox::RejectRole);
  takeBtn->setEnabled(!obsKey.isEmpty());
  giveBtn->setEnabled(!ourKey.isEmpty());
  box.exec();

  QString message;
  if (box.clickedButton() == takeBtn) {
    if (SuiteOutroHotkey::takeOverStopKey(&message)) {
      QMessageBox::information(
          this, tr("Atalho para encerrar a gravação"),
          tr("Pronto: a tecla %1 agora encerra a gravação com transição. O "
             "\"Parar gravação\" do OBS ficou sem tecla, e o botão de parar da "
             "interface continua encerrando na hora.")
              .arg(message));
    } else {
      QMessageBox::warning(this, tr("Atalho para encerrar a gravação"), message);
    }
  } else if (box.clickedButton() == giveBtn) {
    if (SuiteOutroHotkey::giveBackStopKey(&message)) {
      QMessageBox::information(
          this, tr("Atalho para encerrar a gravação"),
          tr("A tecla %1 voltou para o \"Parar gravação\" do OBS.")
              .arg(message));
    } else {
      QMessageBox::warning(this, tr("Atalho para encerrar a gravação"), message);
    }
  }
}
