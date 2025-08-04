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
  logContainer->setVisible(true);
  closeButton->raise();
  
  appendLog("[TAU5] System initializing...");
  appendLog("[BEAM] Starting Erlang VM...");
  
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
      QTimer::singleShot(0, glWidget, [this]() {
        if (glWidget) {
          glWidget->update();
        }
      });
    }
  });
  renderTimer->start(16);
  
  QTimer::singleShot(10000, [this]() {
    if (closeButton && fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
      closeButton->setVisible(true);
      closeButton->raise();
      Logger::log(Logger::Debug, "[LoadingOverlay] Close button shown after 10 seconds");
    }
  });
}

LoadingOverlay::~LoadingOverlay()
{
  if (renderTimer) {
    renderTimer->stop();
    Logger::log(Logger::Debug, "[LoadingOverlay] Render timer stopped in destructor");
  }
  
  Logger::log(Logger::Debug, "[LoadingOverlay] Destructor completed - child widgets cleaned up");
}

void LoadingOverlay::fadeOut()
{
  if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
    Logger::log(Logger::Debug, "[LoadingOverlay] Starting fade out");
    
    raise();
    activateWindow();
    
    if (closeButton) {
      closeButton->setVisible(false);
    }
    
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
    
    Logger::log(Logger::Debug, QString("[LoadingOverlay] Log container positioned at %1,%2 size %3x%4")
                .arg(x).arg(y).arg(logWidth).arg(logHeight));
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
  logWidget->raise();
  logWidget->setVisible(true);
  logWidget->update(); // Force repaint
  
  Logger::log(Logger::Debug, QString("[LoadingOverlay] Log widget visible: %1, geometry: %2x%3, parent visible: %4")
              .arg(logWidget->isVisible())
              .arg(logWidget->width())
              .arg(logWidget->height())
              .arg(isVisible()));
  
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
{
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSwapInterval(1);
  format.setSwapBehavior(QSurfaceFormat::TripleBuffer);
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  setFormat(format);
  
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
  
  logoTexture = new QOpenGLTexture(logoImage.mirrored(false, true));
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
  
  const char *vertexShaderSource = R"(
    #version 120
    attribute vec2 aPos;
    void main() {
      gl_Position = vec4(aPos, 0.0, 1.0);
    }
  )";
  
  const char *fragmentShaderSource = R"(
    #version 120
    uniform float time;
    uniform vec2 resolution;
    uniform sampler2D logoTexture;
    uniform float fadeValue;
    
    #define PI 3.14159265359
    
    mat2 rot(float a) {
      float s = sin(a), c = cos(a);
      return mat2(c, -s, s, c);
    }
    
    float hash(vec2 p) {
      return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }
    
    vec3 toGamma(vec3 col) {
      return pow(col, vec3(0.454545));
    }
    
    vec3 warpEffect(vec2 p, float t) {
      vec3 col = vec3(0.0);
      float centerDist = length(p);
      if(centerDist < 0.35) return col;
      
      vec3 ray = vec3(p * 2.0, 1.0);
      
      float warpActivation = smoothstep(0.0, 2.0, t * 0.35);
      float continuousAccel = t * 0.1;
      float speedMultiplier = 0.5 + warpActivation * 6.5 + continuousAccel * continuousAccel * 2.5;
      float offset = t * 0.02 * speedMultiplier;
      
      float speed2 = warpActivation * 0.6;
      float speed = 0.1 + warpActivation * 0.5;
      offset += sin(offset) * 0.1;
      
      vec3 stp = ray / max(abs(ray.x), abs(ray.y));
      vec3 pos = 2.0 * stp + 0.5;
      
      vec3 deepPinkColor = vec3(0.9, 0.1, 0.5);
      vec3 blueColor = vec3(0.1, 0.4, 1.0);
      float centerFade = smoothstep(0.35, 0.5, centerDist);
      
      int iterations = int(12.0 + min(8.0, continuousAccel * 20.0));
      float brightnessBoost = 1.0 + min(1.5, continuousAccel);
      
      for(int i = 0; i < 20; i++) {
        if(i >= iterations) break;
        float z = hash(floor(pos.xy));
        z = fract(z - offset);
        float d = 30.0 * z - pos.z;
        
        // Early exit if star is too far
        if(d < -1.0 || d > 31.0) {
          pos += stp;
          continue;
        }
        
        // Star shape calculation (circular, not boxy)
        float w = pow(max(0.0, 1.0 - 8.0 * length(fract(pos.xy) - 0.5)), 2.0);
        
        // Color streaks with proper RGB separation for motion blur effect
        vec3 c = max(vec3(0.0), vec3(
          1.0 - abs(d + speed2 * 0.5) / speed,
          1.0 - abs(d) / speed,
          1.0 - abs(d - speed2 * 0.5) / speed
        ));
        
        vec3 starColor = mix(deepPinkColor, blueColor, 0.5 + 0.5 * sin(z * PI));
        c *= starColor;
        
        col += brightnessBoost * (1.0 - z) * c * w * centerFade;
        
        pos += stp;
      }
      
      return toGamma(col) * 0.6;
    }
    
    vec3 cubeWireframe(vec2 p, float logoMask) {
      vec3 col = vec3(0.0);
      float rotSpeed = mix(0.05, 0.15, logoMask);
      float t = time * rotSpeed;
      vec3 angles = vec3(t, t * 0.7, t * 0.3);
      
      vec3 verts[8];
      verts[0] = vec3(-1.0, -1.0, -1.0);
      verts[1] = vec3( 1.0, -1.0, -1.0);
      verts[2] = vec3( 1.0,  1.0, -1.0);
      verts[3] = vec3(-1.0,  1.0, -1.0);
      verts[4] = vec3(-1.0, -1.0,  1.0);
      verts[5] = vec3( 1.0, -1.0,  1.0);
      verts[6] = vec3( 1.0,  1.0,  1.0);
      verts[7] = vec3(-1.0,  1.0,  1.0);
      
      float scale = mix(0.3, 0.35, logoMask);
      vec2 proj[8];
      
      for(int i = 0; i < 8; i++) {
        vec3 v = verts[i];
        v.yz *= rot(angles.x);
        v.xz *= rot(angles.y);
        v.xy *= rot(angles.z);
        proj[i] = v.xy * (2.0 / (4.0 + v.z)) * scale;
      }
      
      float d = 1e10;
      vec2 e;
      
      e = proj[1] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
      e = proj[2] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
      e = proj[3] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
      e = proj[0] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
      e = proj[5] - proj[4]; d = min(d, length(p - proj[4] - e * clamp(dot(p - proj[4], e) / dot(e, e), 0.0, 1.0)));
      e = proj[6] - proj[5]; d = min(d, length(p - proj[5] - e * clamp(dot(p - proj[5], e) / dot(e, e), 0.0, 1.0)));
      e = proj[7] - proj[6]; d = min(d, length(p - proj[6] - e * clamp(dot(p - proj[6], e) / dot(e, e), 0.0, 1.0)));
      e = proj[4] - proj[7]; d = min(d, length(p - proj[7] - e * clamp(dot(p - proj[7], e) / dot(e, e), 0.0, 1.0)));
      e = proj[4] - proj[0]; d = min(d, length(p - proj[0] - e * clamp(dot(p - proj[0], e) / dot(e, e), 0.0, 1.0)));
      e = proj[5] - proj[1]; d = min(d, length(p - proj[1] - e * clamp(dot(p - proj[1], e) / dot(e, e), 0.0, 1.0)));
      e = proj[6] - proj[2]; d = min(d, length(p - proj[2] - e * clamp(dot(p - proj[2], e) / dot(e, e), 0.0, 1.0)));
      e = proj[7] - proj[3]; d = min(d, length(p - proj[3] - e * clamp(dot(p - proj[3], e) / dot(e, e), 0.0, 1.0)));
      
      col += mix(vec3(1.0, 0.65, 0.0), vec3(0.0, 1.0, 0.0), logoMask) * smoothstep(0.02, 0.0, d) * 2.0;
      return col;
    }
    
    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
      
      vec3 col = mix(vec3(0.02, 0.0, 0.05), vec3(0.05, 0.0, 0.1), uv.y * 0.5);
      col += warpEffect(p, time);
      col += vec3(0.05, 0.02, 0.1) * (1.0 - smoothstep(0.0, 1.5, length(p))) * 0.3;
      
      vec2 logoUV = p + 0.5;
      float logoMask = 0.0;
      if(logoUV.x >= 0.0 && logoUV.x <= 1.0 && logoUV.y >= 0.0 && logoUV.y <= 1.0) {
        vec4 logoColor = texture2D(logoTexture, logoUV);
        float lum = dot(logoColor.rgb, vec3(0.299, 0.587, 0.114));
        logoMask = (logoColor.a > 0.5 && lum < 0.5) ? 1.0 : 0.0;
      }
      
      col += cubeWireframe(p, logoMask);
      if(logoMask > 0.5) col = 1.0 - col;
      col *= 1.0 + length(p) * 0.7;
      
      col *= (1.0 - fadeValue);
      
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
  fadeUniform = shaderProgram->uniformLocation("fadeValue");
  
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
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  if (!shaderProgram || !logoTexture || !shaderProgram->isLinked()) {
    return;
  }
  
  shaderProgram->bind();
  
  qreal dpr = devicePixelRatio();
  QSize fbSize = size() * dpr;
  
  float currentTime = timer.elapsed() / 1000.0f;
  
  float frameTime = frameTimer.restart() / 1000.0f;
  if (frameTime > 0.05f) {
    frameTime = 0.016f;
  }
  
  shaderProgram->setUniformValue(timeUniform, currentTime);
  shaderProgram->setUniformValue(resolutionUniform, QVector2D(fbSize.width(), fbSize.height()));
  shaderProgram->setUniformValue(logoTextureUniform, 0);
  
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