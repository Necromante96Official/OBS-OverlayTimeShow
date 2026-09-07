#include "suite-types.hpp"

#include <QObject>

namespace {

QString quoted(const QString &value, const QString &fallback)
{
  if (value.trimmed().isEmpty())
    return fallback;
  return QStringLiteral("\"%1\"").arg(value.trimmed());
}

QString formatDuration(int ms)
{
  if (ms >= 1000) {
    const double seconds = ms / 1000.0;
    QString text = QString::number(seconds, 'f', seconds < 10.0 ? 1 : 0);
    text.replace(QLatin1Char('.'), QLatin1Char(','));
    return QObject::tr("%1 s").arg(text);
  }
  return QObject::tr("%1 ms").arg(ms);
}

QString typeKey(SuiteStepType type)
{
  switch (type) {
  case SuiteStepType::SetScene:
    return QStringLiteral("set_scene");
  case SuiteStepType::SetSourceVisible:
    return QStringLiteral("set_source_visible");
  case SuiteStepType::DelayMs:
    return QStringLiteral("delay_ms");
  case SuiteStepType::SetTransition:
    return QStringLiteral("set_transition");
  case SuiteStepType::SetMute:
    return QStringLiteral("set_mute");
  case SuiteStepType::SetVolume:
    return QStringLiteral("set_volume");
  case SuiteStepType::AudioFade:
    return QStringLiteral("audio_fade");
  case SuiteStepType::IfCurrentScene:
    return QStringLiteral("if_current_scene");
  case SuiteStepType::IfSourceVisible:
    return QStringLiteral("if_source_visible");
  case SuiteStepType::RestartMedia:
    return QStringLiteral("restart_media");
  case SuiteStepType::OpenUrl:
    return QStringLiteral("open_url");
  }
  return QStringLiteral("delay_ms");
}

} // namespace

QString suiteStepTypeToString(SuiteStepType type)
{
  return typeKey(type);
}

SuiteStepType suiteStepTypeFromString(const QString &value)
{
  if (value == QLatin1String("set_scene"))
    return SuiteStepType::SetScene;
  if (value == QLatin1String("set_source_visible"))
    return SuiteStepType::SetSourceVisible;
  if (value == QLatin1String("delay_ms"))
    return SuiteStepType::DelayMs;
  if (value == QLatin1String("set_transition"))
    return SuiteStepType::SetTransition;
  if (value == QLatin1String("set_mute"))
    return SuiteStepType::SetMute;
  if (value == QLatin1String("set_volume"))
    return SuiteStepType::SetVolume;
  if (value == QLatin1String("audio_fade"))
    return SuiteStepType::AudioFade;
  if (value == QLatin1String("if_current_scene"))
    return SuiteStepType::IfCurrentScene;
  if (value == QLatin1String("if_source_visible"))
    return SuiteStepType::IfSourceVisible;
  if (value == QLatin1String("restart_media"))
    return SuiteStepType::RestartMedia;
  if (value == QLatin1String("open_url"))
    return SuiteStepType::OpenUrl;
  return SuiteStepType::DelayMs;
}

QJsonObject SuiteStep::toJson() const
{
  QJsonObject obj;
  obj.insert(QStringLiteral("type"), suiteStepTypeToString(type));
  obj.insert(QStringLiteral("scene"), scene);
  obj.insert(QStringLiteral("source"), source);
  obj.insert(QStringLiteral("transition"), transition);
  obj.insert(QStringLiteral("url"), url);
  obj.insert(QStringLiteral("transitionMs"), transitionMs);
  obj.insert(QStringLiteral("ms"), ms);
  obj.insert(QStringLiteral("visible"), visible);
  obj.insert(QStringLiteral("muted"), muted);
  obj.insert(QStringLiteral("volume"), volume);
  obj.insert(QStringLiteral("fadeAudio"), fadeAudio);
  obj.insert(QStringLiteral("fadeIn"), fadeIn);
  obj.insert(QStringLiteral("fadeMs"), fadeMs);
  obj.insert(QStringLiteral("waitForFade"), waitForFade);
  return obj;
}

SuiteStep SuiteStep::fromJson(const QJsonObject &obj)
{
  SuiteStep step;
  step.type = suiteStepTypeFromString(obj.value(QStringLiteral("type")).toString());
  step.scene = obj.value(QStringLiteral("scene")).toString();
  step.source = obj.value(QStringLiteral("source")).toString();
  step.transition = obj.value(QStringLiteral("transition")).toString();
  step.url = obj.value(QStringLiteral("url")).toString();
  step.transitionMs = obj.value(QStringLiteral("transitionMs")).toInt(300);
  step.ms = obj.value(QStringLiteral("ms")).toInt(1000);
  step.visible = obj.value(QStringLiteral("visible")).toBool(true);
  step.muted = obj.value(QStringLiteral("muted")).toBool(true);
  step.volume = obj.value(QStringLiteral("volume")).toDouble(1.0);
  step.fadeAudio = obj.value(QStringLiteral("fadeAudio")).toBool(false);
  step.fadeIn = obj.value(QStringLiteral("fadeIn")).toBool(true);
  step.fadeMs = obj.value(QStringLiteral("fadeMs")).toInt(500);
  step.waitForFade = obj.value(QStringLiteral("waitForFade")).toBool(true);
  return step;
}

QString SuiteStep::summary() const
{
  switch (type) {
  case SuiteStepType::SetScene: {
    QString text = QObject::tr("Trocar para a cena %1")
                       .arg(quoted(scene, QObject::tr("(cena não escolhida)")));
    if (!transition.trimmed().isEmpty()) {
      text += QObject::tr(", usando a transição %1 de %2")
                  .arg(quoted(transition, QString()), formatDuration(transitionMs));
    }
    return text;
  }
  case SuiteStepType::SetSourceVisible: {
    QString text =
        visible ? QObject::tr("Mostrar a fonte %1 (deixar visível) na cena %2")
                      .arg(quoted(source, QObject::tr("(fonte não escolhida)")),
                           quoted(scene, QObject::tr("(cena não escolhida)")))
                : QObject::tr("Ocultar a fonte %1 (deixar invisível) na cena %2")
                      .arg(quoted(source, QObject::tr("(fonte não escolhida)")),
                           quoted(scene, QObject::tr("(cena não escolhida)")));
    if (fadeAudio) {
      text += visible
                  ? QObject::tr(", com o áudio subindo do silêncio até %1% em %2")
                        .arg(qRound(volume * 100.0))
                        .arg(formatDuration(fadeMs))
                  : QObject::tr(", com o áudio descendo até o silêncio em %1 "
                                "antes de ocultar")
                        .arg(formatDuration(fadeMs));
    }
    return text;
  }
  case SuiteStepType::DelayMs:
    return QObject::tr("Esperar %1 antes do próximo passo").arg(formatDuration(ms));
  case SuiteStepType::SetTransition:
    return QObject::tr("Definir a transição %1 com duração de %2")
        .arg(quoted(transition, QObject::tr("(transição não escolhida)")),
             formatDuration(transitionMs));
  case SuiteStepType::SetMute:
    return muted ? QObject::tr("Silenciar o áudio de %1 (mudo)")
                       .arg(quoted(source, QObject::tr("(fonte não escolhida)")))
                 : QObject::tr("Ativar o áudio de %1 (tirar do mudo)")
                       .arg(quoted(source, QObject::tr("(fonte não escolhida)")));
  case SuiteStepType::SetVolume: {
    QString text = QObject::tr("Ajustar o volume de %1 para %2%")
                       .arg(quoted(source, QObject::tr("(fonte não escolhida)")))
                       .arg(qRound(volume * 100.0));
    if (fadeAudio)
      text += QObject::tr(", deslizando aos poucos em %1").arg(formatDuration(fadeMs));
    else
      text += QObject::tr(", na hora");
    return text;
  }
  case SuiteStepType::AudioFade:
    return fadeIn
               ? QObject::tr("Fade in: subir o áudio de %1 do silêncio até %2% "
                             "em %3")
                     .arg(quoted(source, QObject::tr("(fonte não escolhida)")))
                     .arg(qRound(volume * 100.0))
                     .arg(formatDuration(fadeMs))
               : QObject::tr("Fade out: descer o áudio de %1 até o silêncio em %2")
                     .arg(quoted(source, QObject::tr("(fonte não escolhida)")),
                          formatDuration(fadeMs));
  case SuiteStepType::IfCurrentScene:
    return QObject::tr("Só continuar se a cena atual for %1 (senão, pula o próximo passo)")
        .arg(quoted(scene, QObject::tr("(cena não escolhida)")));
  case SuiteStepType::IfSourceVisible:
    return visible
               ? QObject::tr("Só continuar se a fonte %1 estiver visível (senão, pula o próximo passo)")
                     .arg(quoted(source, QObject::tr("(fonte não escolhida)")))
               : QObject::tr("Só continuar se a fonte %1 estiver oculta (senão, pula o próximo passo)")
                     .arg(quoted(source, QObject::tr("(fonte não escolhida)")));
  case SuiteStepType::RestartMedia:
    return QObject::tr("Reiniciar a mídia %1 desde o começo")
        .arg(quoted(source, QObject::tr("(fonte não escolhida)")));
  case SuiteStepType::OpenUrl:
    return QObject::tr("Abrir %1 no computador")
        .arg(quoted(url, QObject::tr("(endereço não informado)")));
  }
  return QObject::tr("Passo sem descrição");
}

QJsonObject Suite::toJson() const
{
  QJsonObject obj;
  obj.insert(QStringLiteral("id"), id);
  obj.insert(QStringLiteral("name"), name);
  obj.insert(QStringLiteral("openRecordingFolderOnStop"), openRecordingFolderOnStop);
  QJsonArray arr;
  for (const SuiteStep &step : steps)
    arr.append(step.toJson());
  obj.insert(QStringLiteral("steps"), arr);
  return obj;
}

Suite Suite::fromJson(const QJsonObject &obj)
{
  Suite suite;
  suite.id = obj.value(QStringLiteral("id")).toString();
  suite.name = obj.value(QStringLiteral("name")).toString();
  suite.openRecordingFolderOnStop =
      obj.value(QStringLiteral("openRecordingFolderOnStop")).toBool(true);
  const QJsonArray arr = obj.value(QStringLiteral("steps")).toArray();
  for (const QJsonValue &value : arr)
    suite.steps.append(SuiteStep::fromJson(value.toObject()));
  return suite;
}
