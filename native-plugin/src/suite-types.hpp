#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

enum class SuiteStepType {
  SetScene,
  SetSourceVisible,
  DelayMs,
  SetTransition,
  SetMute,
  SetVolume,
  IfCurrentScene,
  IfSourceVisible,
  RestartMedia,
  OpenUrl,
};

struct SuiteStep {
  SuiteStepType type = SuiteStepType::DelayMs;

  QString scene;
  QString source;
  QString transition;
  QString url;

  int transitionMs = 300;
  int ms = 1000;
  bool visible = true;
  bool muted = true;
  double volume = 1.0;

  QJsonObject toJson() const;
  static SuiteStep fromJson(const QJsonObject &obj);
  QString summary() const;
};

struct Suite {
  QString id;
  QString name;
  bool openRecordingFolderOnStop = true;
  QVector<SuiteStep> steps;

  QJsonObject toJson() const;
  static Suite fromJson(const QJsonObject &obj);
};

QString suiteStepTypeToString(SuiteStepType type);
SuiteStepType suiteStepTypeFromString(const QString &value);
