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
#include <QPushButton>
#include <QPointer>
#include <QMouseEvent>

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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
  private:
    QOpenGLShaderProgram *shaderProgram;
    QOpenGLTexture *logoTexture;
    QElapsedTimer timer;
    QElapsedTimer frameTimer;
    int timeUniform;
    int resolutionUniform;
    int logoTextureUniform;
    int fadeUniform;
    int cameraRotationUniform;
    
    // Mouse interaction state
    bool isDragging;
    QPoint lastMousePos;
    float cameraPitch;     // Camera pitch (up/down)
    float cameraYaw;       // Camera yaw (left/right)
    float cameraVelocityX; // Camera pitch velocity
    float cameraVelocityY; // Camera yaw velocity
    
    void createLogoTexture();
  };

private:
  QPointer<GLWidget> glWidget;
  QWidget *logContainer;
  QTextEdit *logWidget;
  QPushButton *closeButton;
  QPropertyAnimation *fadeAnimation;
  QMutex logMutex;
  QStringList logLines;
  QTimer *renderTimer;
  
  static const int MAX_LOG_LINES = 100;
  float fadeToBlackValue;
  
  Q_PROPERTY(float fadeToBlackValue READ getFadeToBlackValue WRITE setFadeToBlackValue)
  
public:
  float getFadeToBlackValue() const { return fadeToBlackValue; }
  void setFadeToBlackValue(float value) { fadeToBlackValue = value; }
};