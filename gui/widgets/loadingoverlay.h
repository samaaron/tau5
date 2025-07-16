#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <QSvgRenderer>
#include <QMutex>
#include <QStringList>

class LoadingOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
  Q_OBJECT

public:
  explicit LoadingOverlay(QWidget *parent = nullptr);
  ~LoadingOverlay();

  void fadeOut();
  void updateGeometry(const QRect &parentGeometry);
  void appendLog(const QString &message);

signals:
  void fadeToBlackComplete();

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

private:
  void createLogoTexture();
  void createTerminalTexture();
  void updateTerminalTexture();

private:
  void startFadeToBlack();

private:
  QPropertyAnimation *fadeAnimation;
  QPropertyAnimation *fadeToBlackAnimation;
  QOpenGLShaderProgram *shaderProgram;
  QOpenGLTexture *logoTexture;
  QOpenGLTexture *terminalTexture;
  QElapsedTimer timer;
  QSvgRenderer *svgRenderer;
  int timeUniform;
  int resolutionUniform;
  int logoTextureUniform;
  int terminalTextureUniform;
  int terminalOffsetUniform;
  int fadeToBlackUniform;
  QMutex logMutex;
  QStringList logLines;
  QStringList pendingLogLines;
  static const int MAX_LOG_LINES = 30;
  float terminalScrollOffset;
  float targetScrollOffset;
  QTimer *updateTimer;
  bool needsTextureUpdate;
  float fadeToBlackValue;
  Q_PROPERTY(float fadeToBlackValue READ getFadeToBlackValue WRITE setFadeToBlackValue)
  
public:
  float getFadeToBlackValue() const { return fadeToBlackValue; }
  void setFadeToBlackValue(float value) { fadeToBlackValue = value; update(); }
};