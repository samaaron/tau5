#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <QSvgRenderer>

class LoadingOverlay : public QOpenGLWidget, protected QOpenGLFunctions
{
  Q_OBJECT

public:
  explicit LoadingOverlay(QWidget *parent = nullptr);
  ~LoadingOverlay();

  void fadeOut();
  void updateGeometry(const QRect &parentGeometry);

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;

private:
  void createLogoTexture();

private:
  QPropertyAnimation *fadeAnimation;
  QOpenGLShaderProgram *shaderProgram;
  QOpenGLTexture *logoTexture;
  QElapsedTimer timer;
  QSvgRenderer *svgRenderer;
  int timeUniform;
  int resolutionUniform;
  int logoTextureUniform;
};