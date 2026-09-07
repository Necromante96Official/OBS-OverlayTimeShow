#include "suite-store.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDir>

SuiteStore::SuiteStore(QObject *parent) : QObject(parent) {}

QString SuiteStore::configPath() const
{
  char *path = obs_module_config_path("suites.json");
  if (!path)
    return {};
  const QString result = QString::fromUtf8(path);
  bfree(path);
  return result;
}

bool SuiteStore::load()
{
  char *dir = obs_module_config_path("");
  if (dir) {
    os_mkdirs(dir);
    bfree(dir);
  }

  m_suites.clear();
  m_activeSuiteId.clear();

  const QString path = configPath();
  if (path.isEmpty())
    return false;

  QFile file(path);
  if (!file.exists()) {
    emit changed();
    return true;
  }
  if (!file.open(QIODevice::ReadOnly))
    return false;

  const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
  m_activeSuiteId = root.value(QStringLiteral("activeSuiteId")).toString();
  const QJsonArray arr = root.value(QStringLiteral("suites")).toArray();
  for (const QJsonValue &value : arr) {
    Suite suite = Suite::fromJson(value.toObject());
    if (suite.id.isEmpty())
      suite.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (suite.name.isEmpty())
      suite.name = QStringLiteral("Suíte");
    m_suites.append(suite);
  }

  if (!m_activeSuiteId.isEmpty() && !suiteById(m_activeSuiteId))
    m_activeSuiteId.clear();

  emit changed();
  return true;
}

bool SuiteStore::save() const
{
  const QString path = configPath();
  if (path.isEmpty())
    return false;

  QJsonObject root;
  root.insert(QStringLiteral("activeSuiteId"), m_activeSuiteId);
  QJsonArray arr;
  for (const Suite &suite : m_suites)
    arr.append(suite.toJson());
  root.insert(QStringLiteral("suites"), arr);

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
  return true;
}

const Suite *SuiteStore::activeSuite() const
{
  return suiteById(m_activeSuiteId);
}

Suite *SuiteStore::suiteById(const QString &id)
{
  for (Suite &suite : m_suites) {
    if (suite.id == id)
      return &suite;
  }
  return nullptr;
}

const Suite *SuiteStore::suiteById(const QString &id) const
{
  for (const Suite &suite : m_suites) {
    if (suite.id == id)
      return &suite;
  }
  return nullptr;
}

Suite SuiteStore::createSuite(const QString &name)
{
  Suite suite;
  suite.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  suite.name = name.trimmed().isEmpty() ? QStringLiteral("Nova suíte") : name.trimmed();
  suite.openRecordingFolderOnStop = true;
  m_suites.append(suite);
  if (m_activeSuiteId.isEmpty())
    m_activeSuiteId = suite.id;
  save();
  emit changed();
  return suite;
}

bool SuiteStore::renameSuite(const QString &id, const QString &name)
{
  Suite *suite = suiteById(id);
  if (!suite)
    return false;
  suite->name = name.trimmed().isEmpty() ? suite->name : name.trimmed();
  save();
  emit changed();
  return true;
}

bool SuiteStore::removeSuite(const QString &id)
{
  for (int i = 0; i < m_suites.size(); ++i) {
    if (m_suites[i].id != id)
      continue;
    m_suites.removeAt(i);
    if (m_activeSuiteId == id)
      m_activeSuiteId = m_suites.isEmpty() ? QString() : m_suites.first().id;
    save();
    emit changed();
    return true;
  }
  return false;
}

Suite SuiteStore::duplicateSuite(const QString &id)
{
  const Suite *src = suiteById(id);
  if (!src)
    return {};
  Suite copy = *src;
  copy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  copy.name = src->name + QStringLiteral(" (cópia)");
  m_suites.append(copy);
  save();
  emit changed();
  return copy;
}

QString SuiteStore::suggestedMergeName(const QStringList &ids) const
{
  QStringList names;
  for (const QString &id : ids) {
    if (const Suite *suite = suiteById(id))
      names.append(suite->name);
  }
  if (names.isEmpty())
    return QStringLiteral("Nova suíte");
  return names.join(QStringLiteral(" + "));
}

Suite SuiteStore::mergeSuites(const QStringList &ids, const QString &name,
                              bool removeOriginals)
{
  QVector<SuiteStep> steps;
  bool openFolder = false;
  int found = 0;

  for (const QString &id : ids) {
    const Suite *suite = suiteById(id);
    if (!suite)
      continue;
    ++found;
    openFolder = openFolder || suite->openRecordingFolderOnStop;
    steps.append(suite->steps);
  }

  if (found < 2)
    return {};

  Suite merged;
  merged.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  merged.name = name.trimmed().isEmpty() ? suggestedMergeName(ids)
                                         : name.trimmed();
  merged.openRecordingFolderOnStop = openFolder;
  merged.steps = steps;
  m_suites.append(merged);

  if (removeOriginals) {
    for (const QString &id : ids) {
      for (int i = 0; i < m_suites.size(); ++i) {
        if (m_suites[i].id != id)
          continue;
        m_suites.removeAt(i);
        break;
      }
    }
  }

  if (m_activeSuiteId.isEmpty() || !suiteById(m_activeSuiteId))
    m_activeSuiteId = merged.id;

  save();
  emit changed();
  return merged;
}

bool SuiteStore::setActiveSuite(const QString &id)
{
  if (id.isEmpty()) {
    m_activeSuiteId.clear();
    save();
    emit changed();
    return true;
  }
  if (!suiteById(id))
    return false;
  m_activeSuiteId = id;
  save();
  emit changed();
  return true;
}

bool SuiteStore::clearActiveSuite()
{
  return setActiveSuite(QString());
}

bool SuiteStore::updateSuite(const Suite &suite)
{
  Suite *dst = suiteById(suite.id);
  if (!dst)
    return false;
  *dst = suite;
  save();
  emit changed();
  return true;
}
