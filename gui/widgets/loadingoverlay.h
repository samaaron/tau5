#pragma once

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QPropertyAnimation>
#include <QElapsedTimer>
#include <QTextEdit>
#include <QMutex>
#include <QStringList>

class LoadingOverlay : public QWidget
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
  void resizeEvent(QResizeEvent *event) override;

private:
  void initializeGL();
  void resizeGL(int w, int h);
  void paintGL();
  
  class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions
  {
  public:
    explicit GLWidget(QWidget *parent = nullptr);
    ~GLWidget();
    
  protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    
  private:
    QOpenGLShaderProgram *shaderProgram;
    QOpenGLTexture *logoTexture;
    QElapsedTimer timer;
    QElapsedTimer frameTimer;  // For frame-independent animation
    float lastFrameTime;
    int timeUniform;
    int resolutionUniform;
    int logoTextureUniform;
    float fadeToBlackValue;
    
    void createLogoTexture();
  };

private:
  GLWidget *glWidget;
  QTextEdit *logWidget;
  QPropertyAnimation *fadeAnimation;
  QPropertyAnimation *fadeToBlackAnimation;
  QMutex logMutex;
  QStringList logLines;
  QTimer *renderTimer;
  
  static const int MAX_LOG_LINES = 100;
  float fadeToBlackValue;
  
  Q_PROPERTY(float fadeToBlackValue READ getFadeToBlackValue WRITE setFadeToBlackValue)
  
public:
  float getFadeToBlackValue() const { return fadeToBlackValue; }
  void setFadeToBlackValue(float value) { fadeToBlackValue = value; }
  
  // Compatibility stubs
  void startFadeToBlack() {}
  bool event(QEvent *event) override { return QWidget::event(event); }
};