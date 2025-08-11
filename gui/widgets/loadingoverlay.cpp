#include "loadingoverlay.h"
#include <QTimer>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QGraphicsBlurEffect>
#include <QVBoxLayout>
#include <QMutexLocker>
#include <QScrollBar>
#include <QResizeEvent>
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <cmath>
#include "../logger.h"
#include "../styles/StyleManager.h"

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(nullptr)
    , glWidget(nullptr)
    , logWidget(nullptr)
    , closeButton(nullptr)
    , fadeAnimation(nullptr)
    , renderTimer(nullptr)
    , fadeToBlackValue(0.0f)
{
  int codiconFontId = QFontDatabase::addApplicationFont(":/fonts/codicon.ttf");
  if (codiconFontId != -1) {
    QStringList codiconFamilies = QFontDatabase::applicationFontFamilies(codiconFontId);
    if (!codiconFamilies.isEmpty()) {
      Logger::log(Logger::Debug, QString("[LoadingOverlay] Loaded codicon font: %1").arg(codiconFamilies.first()));
    }
  }
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  
  glWidget = new GLWidget(this);
  glWidget->setObjectName("glWidget");
  
  logContainer = new QWidget(this);
  logContainer->setObjectName("logContainer");
  
  logContainer->setStyleSheet(QString(R"(
    QWidget#logContainer {
      background-color: %1;
      border: 2px solid %2;
      border-radius: 5px;
      box-shadow: 0 0 20px %3, 0 0 40px %4;
    }
  )").arg(StyleManager::Colors::backgroundPrimaryAlpha(100))
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.8))   // Increased border opacity to 0.8
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.6))   // Stronger inner glow
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.3))); // Outer glow
  
  logWidget = new QTextEdit(logContainer);
  logWidget->setObjectName("logWidget");
  logWidget->setReadOnly(true);
  logWidget->setFrameStyle(QFrame::NoFrame);
  logWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  logWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  
  logWidget->setAttribute(Qt::WA_TranslucentBackground);
  
  logWidget->setStyleSheet(QString(R"(
    QTextEdit#logWidget {
      background-color: transparent;
      color: %1;
      font-family: 'Cascadia Code', 'Cascadia Mono', 'Consolas', monospace;
      font-size: 9px;
      font-weight: 600;
      padding: 12px;
      border: none;
      text-shadow: 0 0 3px %2, 0 0 6px %3;
    }
    QTextEdit#logWidget::selection {
      background-color: %4;
      color: %5;
    }
  )").arg(StyleManager::Colors::accentPrimaryAlpha(1.0))     // Full brightness text
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.8))     // Strong inner text glow
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.4))     // Outer text glow
     .arg(StyleManager::Colors::accentPrimaryAlpha(0.4))     // Brighter selection
     .arg(StyleManager::Colors::TEXT_PRIMARY));
  
  QVBoxLayout *containerLayout = new QVBoxLayout(logContainer);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->addWidget(logWidget);
  
  closeButton = new QPushButton(QChar(0xEA76), this);
  closeButton->setObjectName("closeButton");
  closeButton->setFixedSize(36, 36);
  closeButton->setCursor(Qt::PointingHandCursor);
  closeButton->setStyleSheet(QString(R"(
    QPushButton#closeButton {
      background-color: %1;
      color: %2;
      border: 1px solid %3;
      border-radius: 4px;
      font-family: 'codicon';
      font-size: 16px;
      font-weight: normal;
      padding: 0px;
      text-align: center;
    }
    QPushButton#closeButton:hover {
      background-color: %4;
      color: %5;
      border: 1px solid %6;
    }
    QPushButton#closeButton:pressed {
      background-color: %7;
      color: %8;
      border: 1px solid %9;
    }
  )")
    .arg(StyleManager::Colors::backgroundPrimaryAlpha(0.9))
    .arg(StyleManager::Colors::ACCENT_PRIMARY)
    .arg(StyleManager::Colors::ACCENT_PRIMARY)
    .arg(StyleManager::Colors::ACCENT_PRIMARY)
    .arg(StyleManager::Colors::BACKGROUND_PRIMARY)
    .arg(StyleManager::Colors::ACCENT_PRIMARY)
    .arg(StyleManager::Colors::accentPrimaryAlpha(0.8))
    .arg(StyleManager::Colors::BACKGROUND_PRIMARY)
    .arg(StyleManager::Colors::accentPrimaryAlpha(0.8))
  );
  
  closeButton->setVisible(false);
  
  connect(closeButton, &QPushButton::clicked, [this]() {
    QApplication::quit();
  });
  
  glWidget->stackUnder(logContainer);
  logContainer->raise();
  closeButton->raise();
  
  appendLog("[TAU5] System initializing...\n[BEAM] Starting Erlang VM...");
  
  fadeAnimation = new QPropertyAnimation(this, "fadeToBlackValue", this);
  fadeAnimation->setDuration(1000);
  fadeAnimation->setStartValue(0.0);
  fadeAnimation->setEndValue(1.0);
  fadeAnimation->setEasingCurve(QEasingCurve::InQuad);
  
  connect(fadeAnimation, &QPropertyAnimation::valueChanged, [this]() {
    if (glWidget) {
      glWidget->update();
    }
    update();
  });
  
  connect(fadeAnimation, &QPropertyAnimation::finished, this, [this]() {
    emit fadeToBlackComplete();
    QTimer::singleShot(50, this, &QWidget::close);
  });
  
  renderTimer = new QTimer(this);
  renderTimer->setTimerType(Qt::PreciseTimer);
  connect(renderTimer, &QTimer::timeout, [this]() {
    if (glWidget) {
      glWidget->update();
    }
  });
  renderTimer->start(16);  // 60 FPS for smooth animation
  
  QTimer::singleShot(10000, [this]() {
    if (closeButton && fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
      closeButton->setVisible(true);
      closeButton->raise();
    }
  });
}

LoadingOverlay::~LoadingOverlay()
{
  if (renderTimer) {
    renderTimer->stop();
  }
}

void LoadingOverlay::fadeOut()
{
  if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
    
    raise();
    activateWindow();
    
    if (closeButton) {
      closeButton->setVisible(false);
    }
    
    // Log lines will be hidden by fade anyway
    
    fadeAnimation->start();
  }
}

void LoadingOverlay::updateGeometry(const QRect &parentGeometry)
{
  setGeometry(parentGeometry);
  
  QResizeEvent event(size(), size());
  resizeEvent(&event);
}

void LoadingOverlay::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  
  if (glWidget) {
    glWidget->setGeometry(0, 0, width(), height());
  }
  
  if (logContainer) {
    int logWidth = qMin(500, width() / 3);
    int logHeight = qMin(300, height() / 3);
    int x = width() - logWidth - 20;
    int y = height() - logHeight - 20;
    logContainer->setGeometry(x, y, logWidth, logHeight);
  }
  
  if (closeButton) {
    closeButton->move(width() - closeButton->width() - 10, 10);
  }
}

void LoadingOverlay::appendLog(const QString &message)
{
  QMutexLocker locker(&logMutex);
  
  // Don't log the appended content to avoid duplicating server logs in GUI log
  
  QStringList lines = message.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      logLines.append(trimmed);
      while (logLines.size() > MAX_LOG_LINES) {
        logLines.removeFirst();
      }
    }
  }
  
  QString logText = logLines.join("\n");
  logWidget->setPlainText(logText);
  
  QScrollBar *sb = logWidget->verticalScrollBar();
  if (sb) {
    sb->setValue(sb->maximum());
  }
}

// GLWidget implementation

LoadingOverlay::GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
    , shaderProgram(nullptr)
    , logoTexture(nullptr)
    , isDragging(false)
    , lastMousePos(0, 0)
    , cameraPitch(0.0f)
    , cameraYaw(0.0f)
    , cameraVelocityX(0.0f)
    , cameraVelocityY(0.0f)
{
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSwapInterval(1);
  format.setSwapBehavior(QSurfaceFormat::TripleBuffer);
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  setFormat(format);
  
  // Enable mouse tracking to get hover cursor changes
  setMouseTracking(true);
  setCursor(Qt::OpenHandCursor);
}

LoadingOverlay::GLWidget::~GLWidget()
{
  if (context() && context()->isValid()) {
    makeCurrent();
    delete shaderProgram;
    delete logoTexture;
    doneCurrent();
  }
}

void LoadingOverlay::GLWidget::createLogoTexture()
{
  makeCurrent();
  
  if (logoTexture) {
    delete logoTexture;
    logoTexture = nullptr;
  }
  
  QImage logoImage(":/images/tau5-bw-hirez.png");
  if (logoImage.isNull()) {
    Logger::log(Logger::Warning, "[LoadingOverlay] Failed to load logo image");
    logoImage = QImage(512, 512, QImage::Format_ARGB32);
    logoImage.fill(Qt::white);
  }
  
  logoTexture = new QOpenGLTexture(logoImage.mirrored(false, true));
  logoTexture->setMinificationFilter(QOpenGLTexture::Linear);
  logoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
  logoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
  
  doneCurrent();
}

void LoadingOverlay::GLWidget::initializeGL()
{
  initializeOpenGLFunctions();
  
  Logger::log(Logger::Info, QString("[LoadingOverlay] OpenGL version: %1")
              .arg(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
  
  timer.start();
  frameTimer.start();
  
  // Load shaders from Qt resource system (compiled into executable)
  QFile vertFile(":/shaders/tau5-loading.vert");
  QFile fragFile(":/shaders/tau5-loading.frag");
  
  // Load vertex shader
  if (!vertFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Failed to load vertex shader from: %1").arg(vertFile.fileName()));
    return;
  }
  QString vertexShaderSource = "#version 120\n" + QTextStream(&vertFile).readAll();
  vertFile.close();
  Logger::log(Logger::Debug, "[LoadingOverlay] Loaded vertex shader from file");
  
  // Load fragment shader
  if (!fragFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Failed to load fragment shader from: %1").arg(fragFile.fileName()));
    return;
  }
  QString fragmentShaderSource = "#version 120\n" + QTextStream(&fragFile).readAll();
  fragFile.close();
  Logger::log(Logger::Debug, "[LoadingOverlay] Loaded fragment shader from file");
  
  shaderProgram = new QOpenGLShaderProgram(this);
  
  auto addShader = [this](QOpenGLShader::ShaderType type, const QString& source, const char* name) {
    if (!shaderProgram->addShaderFromSourceCode(type, source.toUtf8().constData())) {
      Logger::log(Logger::Error, QString("[LoadingOverlay] %1 shader error: %2")
                  .arg(name).arg(shaderProgram->log()));
      return false;
    }
    return true;
  };
  
  bool vertexOk = addShader(QOpenGLShader::Vertex, vertexShaderSource, "Vertex");
  bool fragmentOk = addShader(QOpenGLShader::Fragment, fragmentShaderSource, "Fragment");
  
  if (vertexOk && fragmentOk && !shaderProgram->link()) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Shader link error: %1").arg(shaderProgram->log()));
  }
  
  timeUniform = shaderProgram->uniformLocation("time");
  resolutionUniform = shaderProgram->uniformLocation("resolution");
  logoTextureUniform = shaderProgram->uniformLocation("logoTexture");
  fadeUniform = shaderProgram->uniformLocation("fadeValue");
  cameraRotationUniform = shaderProgram->uniformLocation("cameraRotation");
  
  createLogoTexture();
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void LoadingOverlay::GLWidget::resizeGL(int w, int h)
{
  qreal dpr = devicePixelRatio();
  int actualWidth = static_cast<int>(w * dpr);
  int actualHeight = static_cast<int>(h * dpr);
  glViewport(0, 0, actualWidth, actualHeight);
}

void LoadingOverlay::GLWidget::paintGL()
{
  if (!isVisible()) {
    return;
  }
  
  // Update physics when not dragging
  if (!isDragging) {
    // Apply inertia to camera rotation
    cameraPitch += cameraVelocityX;
    cameraYaw += cameraVelocityY;
    
    // Apply damping to camera velocity
    const float damping = 0.985f;
    cameraVelocityX *= damping;
    cameraVelocityY *= damping;
    
    // Stop when velocity is very small
    const float minVelocity = 0.00001f;
    if (std::abs(cameraVelocityX) < minVelocity) cameraVelocityX = 0.0f;
    if (std::abs(cameraVelocityY) < minVelocity) cameraVelocityY = 0.0f;
  }
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  if (!shaderProgram || !logoTexture || !shaderProgram->isLinked()) {
    return;
  }
  
  shaderProgram->bind();
  
  qreal dpr = devicePixelRatio();
  QSize fbSize = size() * dpr;
  
  float currentTime = timer.elapsed() / 1000.0f;
  
  frameTimer.restart();  // Just restart for next frame
  
  shaderProgram->setUniformValue(timeUniform, currentTime);
  shaderProgram->setUniformValue(resolutionUniform, QVector2D(fbSize.width(), fbSize.height()));
  shaderProgram->setUniformValue(logoTextureUniform, 0);
  
  // Pass camera rotation to shader
  shaderProgram->setUniformValue(cameraRotationUniform, QVector2D(cameraPitch, cameraYaw));
  
  LoadingOverlay* parent = qobject_cast<LoadingOverlay*>(parentWidget());
  float fadeValue = parent ? parent->getFadeToBlackValue() : 0.0f;
  shaderProgram->setUniformValue(fadeUniform, fadeValue);
  
  glActiveTexture(GL_TEXTURE0);
  logoTexture->bind();
  
  static const GLfloat verts[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
  };
  
  int vertexLoc = shaderProgram->attributeLocation("aPos");
  shaderProgram->enableAttributeArray(vertexLoc);
  shaderProgram->setAttributeArray(vertexLoc, verts, 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  shaderProgram->disableAttributeArray(vertexLoc);
  
  shaderProgram->release();
}

void LoadingOverlay::GLWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    isDragging = true;
    lastMousePos = event->pos();
    cameraVelocityX = 0.0f;
    cameraVelocityY = 0.0f;
    setCursor(Qt::ClosedHandCursor);
  }
}

void LoadingOverlay::GLWidget::mouseMoveEvent(QMouseEvent *event)
{
  if (isDragging && (event->buttons() & Qt::LeftButton)) {
    QPoint delta = event->pos() - lastMousePos;
    
    // Camera rotation - perfect screen-space control
    const float rotationSpeed = 0.01f;
    
    // Update camera velocities (both axes inverted for consistency with web version)
    cameraVelocityX = -delta.y() * rotationSpeed;  // Vertical drag -> pitch (inverted)
    cameraVelocityY = delta.x() * rotationSpeed;   // Horizontal drag -> yaw (inverted)
    
    // Apply camera rotations
    cameraPitch += cameraVelocityX;
    cameraYaw += cameraVelocityY;
    
    lastMousePos = event->pos();
    update(); // Request a repaint
  }
}

void LoadingOverlay::GLWidget::mouseReleaseEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    isDragging = false;
    setCursor(Qt::OpenHandCursor);
  }
}