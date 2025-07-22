#include "loadingoverlay.h"
#include <QTimer>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QVBoxLayout>
#include <QMutexLocker>
#include <QScrollBar>
#include <QResizeEvent>
#include "../logger.h"
#include "../styles/StyleManager.h"

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QWidget(nullptr)
    , glWidget(nullptr)
    , logWidget(nullptr)
    , fadeAnimation(nullptr)
    , fadeToBlackAnimation(nullptr)
    , renderTimer(nullptr)
    , fadeToBlackValue(0.0f)
{
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  
  // Create GL widget for background
  glWidget = new GLWidget(this);
  glWidget->setObjectName("glWidget");
  
  // Create log widget overlay
  logWidget = new QTextEdit(this);
  logWidget->setObjectName("logWidget");
  logWidget->setReadOnly(true);
  logWidget->setFrameStyle(QFrame::NoFrame);
  logWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  logWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  
  // Style the log widget with explicit opacity
  logWidget->setStyleSheet(QString(R"(
    QTextEdit#logWidget {
      background-color: rgba(20, 20, 20, 230);
      color: rgb(255, 165, 0);
      font-family: 'Cascadia Code', 'Cascadia Mono', 'Consolas', monospace;
      font-size: 9px;
      padding: 10px;
      border: 1px solid rgba(255, 165, 0, 0.3);
      border-radius: 5px;
    }
  )"));
  
  // Ensure proper stacking order
  glWidget->stackUnder(logWidget);
  logWidget->raise();
  logWidget->setVisible(true);
  
  // Add initial test logs
  appendLog("[TAU5] System initializing...");
  appendLog("[BEAM] Starting Erlang VM...");
  
  // Setup fade animation without graphics effect to avoid caching
  fadeAnimation = new QPropertyAnimation(this, "windowOpacity", this);
  fadeAnimation->setDuration(500);
  fadeAnimation->setStartValue(1.0);
  fadeAnimation->setEndValue(0.0);
  
  connect(fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::close);
  
  // Use high-priority timer that yields to event loop
  renderTimer = new QTimer(this);
  renderTimer->setTimerType(Qt::PreciseTimer);
  connect(renderTimer, &QTimer::timeout, [this]() {
    if (glWidget) {
      // Use zero-delay single shot to ensure the update happens
      // even when the main thread is busy
      QTimer::singleShot(0, glWidget, [this]() {
        if (glWidget) {
          glWidget->update();
        }
      });
    }
  });
  renderTimer->start(16); // 60 FPS target
}

LoadingOverlay::~LoadingOverlay()
{
  // Stop the render timer when the widget is destroyed
  if (renderTimer) {
    renderTimer->stop();
    Logger::log(Logger::Debug, "[LoadingOverlay] Render timer stopped in destructor");
  }
}

void LoadingOverlay::fadeOut()
{
  if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
    Logger::log(Logger::Debug, "[LoadingOverlay] Starting fade out");
    // Don't stop the render timer here - let it run during fade
    {
      QMutexLocker locker(&logMutex);
      logLines.clear();
    }
    fadeAnimation->start();
  }
}

void LoadingOverlay::updateGeometry(const QRect &parentGeometry)
{
  setGeometry(parentGeometry);
  
  // Manually trigger resize event to position widgets
  QResizeEvent event(size(), size());
  resizeEvent(&event);
}

void LoadingOverlay::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  
  // Resize GL widget to fill the window
  if (glWidget) {
    glWidget->setGeometry(0, 0, width(), height());
  }
  
  // Position log widget in bottom-right corner
  if (logWidget) {
    int logWidth = qMin(500, width() / 3);
    int logHeight = qMin(300, height() / 3);
    int x = width() - logWidth - 20;
    int y = height() - logHeight - 20;
    logWidget->setGeometry(x, y, logWidth, logHeight);
    logWidget->raise(); // Ensure it's on top
    
    Logger::log(Logger::Debug, QString("[LoadingOverlay] Log widget positioned at %1,%2 size %3x%4")
                .arg(x).arg(y).arg(logWidth).arg(logHeight));
  }
}

void LoadingOverlay::appendLog(const QString &message)
{
  QMutexLocker locker(&logMutex);
  
  Logger::log(Logger::Debug, QString("[LoadingOverlay] appendLog: %1").arg(message));
  
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
  
  // Update log widget
  QString logText = logLines.join("\n");
  logWidget->setPlainText(logText);
  logWidget->raise();
  logWidget->setVisible(true);
  logWidget->update(); // Force repaint
  
  // Debug - check if widget is visible and parent
  Logger::log(Logger::Debug, QString("[LoadingOverlay] Log widget visible: %1, geometry: %2x%3, text: '%4', parent visible: %5")
              .arg(logWidget->isVisible())
              .arg(logWidget->width())
              .arg(logWidget->height())
              .arg(logText.left(50))
              .arg(isVisible()));
  
  // Auto-scroll to bottom
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
    , lastFrameTime(0.0f)
    , fadeToBlackValue(0.0f)
{
  // Enable VSync and triple buffering to reduce tearing
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSwapInterval(1); // VSync
  format.setSwapBehavior(QSurfaceFormat::TripleBuffer); // Triple buffering
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  setFormat(format);
  
  // Enable threaded rendering
  QSurfaceFormat::setDefaultFormat(format);
}

LoadingOverlay::GLWidget::~GLWidget()
{
  makeCurrent();
  delete shaderProgram;
  delete logoTexture;
  doneCurrent();
}

void LoadingOverlay::GLWidget::createLogoTexture()
{
  if (logoTexture) {
    delete logoTexture;
  }
  
  QImage logoImage(":/images/tau5-bw-hirez.png");
  if (logoImage.isNull()) {
    Logger::log(Logger::Warning, "[LoadingOverlay] Failed to load logo image");
    logoImage = QImage(512, 512, QImage::Format_ARGB32);
    logoImage.fill(Qt::white);
  }
  
  logoTexture = new QOpenGLTexture(logoImage.flipped(Qt::Vertical));
  logoTexture->setMinificationFilter(QOpenGLTexture::Linear);
  logoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
  logoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void LoadingOverlay::GLWidget::initializeGL()
{
  initializeOpenGLFunctions();
  
  Logger::log(Logger::Info, QString("[LoadingOverlay] OpenGL version: %1")
              .arg(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
  
  timer.start();
  frameTimer.start();
  
  // Simple vertex shader
  const char *vertexShaderSource = R"(
    #version 120
    attribute vec2 aPos;
    void main() {
      gl_Position = vec4(aPos, 0.0, 1.0);
    }
  )";
  
  // Ultra-simple performant fragment shader
  const char *fragmentShaderSource = R"(
    #version 120
    uniform float time;
    uniform vec2 resolution;
    uniform sampler2D logoTexture;
    
    #define PI 3.14159265359
    
    mat2 rot(float a) {
      float s = sin(a), c = cos(a);
      return mat2(c, -s, s, c);
    }
    
    vec3 cubeWireframe(vec2 p, float logoMask) {
      vec3 col = vec3(0.0);
      
      // Slow rotation, faster when over logo
      float rotSpeed = mix(0.05, 0.15, logoMask);
      float t = time * rotSpeed;
      vec3 angles = vec3(t, t * 0.7, t * 0.3);
      
      // Cube vertices
      vec3 verts[8];
      verts[0] = vec3(-1.0, -1.0, -1.0);
      verts[1] = vec3( 1.0, -1.0, -1.0);
      verts[2] = vec3( 1.0,  1.0, -1.0);
      verts[3] = vec3(-1.0,  1.0, -1.0);
      verts[4] = vec3(-1.0, -1.0,  1.0);
      verts[5] = vec3( 1.0, -1.0,  1.0);
      verts[6] = vec3( 1.0,  1.0,  1.0);
      verts[7] = vec3(-1.0,  1.0,  1.0);
      
      // Scale cube when over logo
      float scale = mix(0.3, 0.35, logoMask);
      
      // Project vertices
      vec2 proj[8];
      for(int i = 0; i < 8; i++) {
        vec3 v = verts[i];
        v.yz *= rot(angles.x);
        v.xz *= rot(angles.y);
        v.xy *= rot(angles.z);
        float z = 4.0 + v.z;
        proj[i] = v.xy * (2.0 / z) * scale;
      }
      
      // Draw edges (simplified)
      float d = 1e10;
      
      // Front face
      vec2 e;
      e = proj[1] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
      e = proj[2] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
      e = proj[3] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
      e = proj[0] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
      
      // Back face
      e = proj[5] - proj[4]; d = min(d, length(p - proj[4] - e * clamp(dot(p - proj[4], e) / dot(e, e), 0.0, 1.0)));
      e = proj[6] - proj[5]; d = min(d, length(p - proj[5] - e * clamp(dot(p - proj[5], e) / dot(e, e), 0.0, 1.0)));
      e = proj[7] - proj[6]; d = min(d, length(p - proj[6] - e * clamp(dot(p - proj[6], e) / dot(e, e), 0.0, 1.0)));
      e = proj[4] - proj[7]; d = min(d, length(p - proj[7] - e * clamp(dot(p - proj[7], e) / dot(e, e), 0.0, 1.0)));
      
      // Connecting edges
      e = proj[4] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
      e = proj[5] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
      e = proj[6] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
      e = proj[7] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
      
      // Use color that inverts to deep neon pink when over logo, orange otherwise
      // Deep neon pink (1.0, 0.0, 1.0) inverted = (0.0, 1.0, 0.0) = pure green
      // For a slightly warmer neon pink (1.0, 0.1, 0.9) inverted = (0.0, 0.9, 0.1) = green with slight yellow
      vec3 cubeColor = mix(vec3(1.0, 0.65, 0.0), vec3(0.0, 1.0, 0.0), logoMask);
      col += cubeColor * smoothstep(0.02, 0.0, d) * 2.0;
      
      return col;
    }
    
    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
      
      // Simple gradient background
      vec3 col = mix(vec3(0.05, 0.0, 0.1), vec3(0.1, 0.0, 0.2), uv.y);
      
      // Check logo area
      vec2 logoUV = p + 0.5;
      float logoMask = 0.0;
      if(logoUV.x >= 0.0 && logoUV.x <= 1.0 && logoUV.y >= 0.0 && logoUV.y <= 1.0) {
        vec4 logoColor = texture2D(logoTexture, logoUV);
        float lum = dot(logoColor.rgb, vec3(0.299, 0.587, 0.114));
        logoMask = (logoColor.a > 0.5 && lum < 0.5) ? 1.0 : 0.0;
      }
      
      // Draw cube
      col += cubeWireframe(p, logoMask);
      
      // Draw logo (inverted)
      if(logoMask > 0.5) {
        col = 1.0 - col;
      }
      
      // Vignette
      col *= 1.0 - length(p) * 0.5;
      
      gl_FragColor = vec4(col, 1.0);
    }
  )";
  
  shaderProgram = new QOpenGLShaderProgram(this);
  if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Vertex shader error: %1").arg(shaderProgram->log()));
  }
  if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Fragment shader error: %1").arg(shaderProgram->log()));
  }
  if (!shaderProgram->link()) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Shader link error: %1").arg(shaderProgram->log()));
  }
  
  timeUniform = shaderProgram->uniformLocation("time");
  resolutionUniform = shaderProgram->uniformLocation("resolution");
  logoTextureUniform = shaderProgram->uniformLocation("logoTexture");
  
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
  // Skip rendering if not visible
  if (!isVisible()) {
    return;
  }
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  if (!shaderProgram || !logoTexture || !shaderProgram->isLinked()) {
    return;
  }
  
  shaderProgram->bind();
  
  qreal dpr = devicePixelRatio();
  QSize fbSize = size() * dpr;
  
  // Smooth time progression - use accumulated time
  float currentTime = timer.elapsed() / 1000.0f;
  
  // Apply exponential smoothing if frame time varies too much
  float frameTime = frameTimer.restart() / 1000.0f;
  if (frameTime > 0.05f) { // If frame took more than 50ms
    frameTime = 0.016f; // Cap at 60fps equivalent
  }
  
  shaderProgram->setUniformValue(timeUniform, currentTime);
  shaderProgram->setUniformValue(resolutionUniform, QVector2D(fbSize.width(), fbSize.height()));
  shaderProgram->setUniformValue(logoTextureUniform, 0);
  
  glActiveTexture(GL_TEXTURE0);
  logoTexture->bind();
  
  // Full screen quad
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