#include "suite-types.hpp"

#include <QObject>

namespace SuiteTypesDetail {

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

} // namespace SuiteTypesDetail

QString suiteStepTypeToString(SuiteStepType type)
{
  return SuiteTypesDetail::typeKey(type);
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

  switch (type) {
  case SuiteStepType::SetScene:
    obj.insert(QStringLiteral("scene"), scene);
    obj.insert(QStringLiteral("transition"), transition);
    obj.insert(QStringLiteral("transitionMs"), transitionMs);
    break;
  case SuiteStepType::SetSourceVisible:
    obj.insert(QStringLiteral("scene"), scene);
    obj.insert(QStringLiteral("source"), source);
    obj.insert(QStringLiteral("visible"), visible);
    break;
  case SuiteStepType::DelayMs:
    obj.insert(QStringLiteral("ms"), ms);
    break;
  case SuiteStepType::SetTransition:
    obj.insert(QStringLiteral("transition"), transition);
    obj.insert(QStringLiteral("transitionMs"), transitionMs);
    break;
  case SuiteStepType::SetMute:
    obj.insert(QStringLiteral("source"), source);
    obj.insert(QStringLiteral("muted"), muted);
    break;
  case SuiteStepType::SetVolume:
    obj.insert(QStringLiteral("source"), source);
    obj.insert(QStringLiteral("volume"), volume);
    break;
  case SuiteStepType::IfCurrentScene:
    obj.insert(QStringLiteral("scene"), scene);
    break;
  case SuiteStepType::IfSourceVisible:
    obj.insert(QStringLiteral("scene"), scene);
    obj.insert(QStringLiteral("source"), source);
    obj.insert(QStringLiteral("visible"), visible);
    break;
  case SuiteStepType::RestartMedia:
    obj.insert(QStringLiteral("source"), source);
    break;
  case SuiteStepType::OpenUrl:
    obj.insert(QStringLiteral("url"), url);
    break;
  }
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
  return step;
}

QString SuiteStep::summary() const
{
  switch (type) {
  case SuiteStepType::SetScene:
    return QObject::tr("Cena: %1").arg(scene);
  case SuiteStepType::SetSourceVisible:
    return QObject::tr("Fonte %1: %2")
        .arg(source, visible ? QObject::tr("mostrar") : QObject::tr("ocultar"));
  case SuiteStepType::DelayMs:
    return QObject::tr("Esperar %1 ms").arg(ms);
  case SuiteStepType::SetTransition:
    return QObject::tr("Transicao: %1 (%2 ms)").arg(transition).arg(transitionMs);
  case SuiteStepType::SetMute:
    return QObject::tr("Audio %1: %2")
        .arg(source, muted ? QObject::tr("mute") : QObject::tr("unmute"));
  case SuiteStepType::SetVolume:
    return QObject::tr("Volume %1: %2%")
        .arg(source)
        .arg(qRound(volume * 100.0));
  case SuiteStepType::IfCurrentScene:
    return QObject::tr("Se cena atual = %1").arg(scene);
  case SuiteStepType::IfSourceVisible:
    return QObject::tr("Se fonte %1 %2")
        .arg(source, visible ? QObject::tr("visivel") : QObject::tr("oculta"));
  case SuiteStepType::RestartMedia:
    return QObject::tr("Reiniciar midia: %1").arg(source);
  case SuiteStepType::OpenUrl:
    return QObject::tr("Abrir: %1").arg(url);
  }
  return QObject::tr("Passo");
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
