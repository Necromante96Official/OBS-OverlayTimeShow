#pragma once

#include "suite-types.hpp"

#include <QObject>
#include <QVector>

class SuiteStore : public QObject {
  Q_OBJECT

public:
  explicit SuiteStore(QObject *parent = nullptr);

  bool load();
  bool save() const;

  QString configPath() const;

  const QVector<Suite> &suites() const { return m_suites; }
  QString activeSuiteId() const { return m_activeSuiteId; }

  const Suite *activeSuite() const;
  Suite *suiteById(const QString &id);
  const Suite *suiteById(const QString &id) const;

  Suite createSuite(const QString &name);
  bool renameSuite(const QString &id, const QString &name);
  bool removeSuite(const QString &id);
  Suite duplicateSuite(const QString &id);
  bool setActiveSuite(const QString &id);
  bool clearActiveSuite();
  bool updateSuite(const Suite &suite);

signals:
  void changed();

private:
  QVector<Suite> m_suites;
  QString m_activeSuiteId;
};
