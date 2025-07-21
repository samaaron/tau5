#include "loadingoverlay.h"
#include <QTimer>
#include <QPainter>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <QMutexLocker>
#include <QTransform>
#include <QScreen>
#include <QWindow>
#include "../logger.h"
#include "../styles/StyleManager.h"

LoadingOverlay::LoadingOverlay(QWidget *parent)
    : QOpenGLWidget(nullptr)
    , fadeAnimation(nullptr)
    , fadeToBlackAnimation(nullptr)
    , shaderProgram(nullptr)
    , logoTexture(nullptr)
    , terminalTexture(nullptr)
    , svgRenderer(nullptr)
    , terminalScrollOffset(0.0f)
    , targetScrollOffset(0.0f)
    , updateTimer(nullptr)
    , renderTimer(nullptr)
    , needsTextureUpdate(false)
    , fadeToBlackValue(0.0f)
{
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  
  fadeAnimation = new QPropertyAnimation(this, "windowOpacity", this);
  fadeAnimation->setDuration(500);
  fadeAnimation->setStartValue(1.0);
  fadeAnimation->setEndValue(0.0);
  
  connect(fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::close);
  
  setWindowOpacity(1.0);
  
  svgRenderer = nullptr;
  
  renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, QOverload<>::of(&QOpenGLWidget::update));
  renderTimer->start(33);
  
  updateTimer = new QTimer(this);
  connect(updateTimer, &QTimer::timeout, this, [this]() {
    if (needsTextureUpdate) {
      updateTerminalTexture();
      needsTextureUpdate = false;
    }
  });
  updateTimer->start(100);
}

LoadingOverlay::~LoadingOverlay()
{
  makeCurrent();
  delete shaderProgram;
  delete logoTexture;
  delete terminalTexture;
  doneCurrent();
}

void LoadingOverlay::fadeOut()
{
  if (fadeAnimation && fadeAnimation->state() != QAbstractAnimation::Running) {
    // Stop timers to save resources
    if (renderTimer) {
      renderTimer->stop();
    }
    if (updateTimer) {
      updateTimer->stop();
    }
    {
      QMutexLocker locker(&logMutex);
      logLines.clear();
      pendingLogLines.clear();
    }
    
    fadeAnimation->start();
  }
}

void LoadingOverlay::startFadeToBlack()
{
  if (fadeToBlackAnimation && fadeToBlackAnimation->state() != QAbstractAnimation::Running) {
    fadeToBlackAnimation->start();
  }
}

void LoadingOverlay::updateGeometry(const QRect &parentGeometry)
{
  setGeometry(parentGeometry);
}

void LoadingOverlay::createLogoTexture()
{
  if (logoTexture) {
    delete logoTexture;
  }
  
  QImage logoImage(":/images/tau5-bw-hirez.png");
  if (logoImage.isNull()) {
    logoImage = QImage(512, 512, QImage::Format_ARGB32);
    logoImage.fill(Qt::white);
  }
  
  logoTexture = new QOpenGLTexture(logoImage.flipped(Qt::Vertical));
  logoTexture->setMinificationFilter(QOpenGLTexture::Linear);
  logoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
  logoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
}

void LoadingOverlay::createTerminalTexture()
{
  if (terminalTexture) {
    delete terminalTexture;
  }
  
  QImage terminalImage(2048, 1789, QImage::Format_ARGB32);
  terminalImage.fill(Qt::black);
  
  terminalTexture = new QOpenGLTexture(terminalImage);
  terminalTexture->setMinificationFilter(QOpenGLTexture::Linear);
  terminalTexture->setMagnificationFilter(QOpenGLTexture::Linear);
  terminalTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
  
  logLines.append("[TAU5] System initializing...");
  logLines.append("[BEAM] Starting Erlang VM...");
  updateTerminalTexture();
}

void LoadingOverlay::updateTerminalTexture()
{
  if (!terminalTexture || !isValid()) return;
  
  {
    QMutexLocker locker(&logMutex);
    if (!pendingLogLines.isEmpty()) {
      logLines.append(pendingLogLines);
      pendingLogLines.clear();
      
      while (logLines.size() > MAX_LOG_LINES) {
        logLines.removeFirst();
      }
    }
  }
  
  QImage terminalImage(2048, 1789, QImage::Format_ARGB32);
  terminalImage.fill(Qt::black);
  
  QPainter painter(&terminalImage);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::Antialiasing, true);
  
  QFont font("Cascadia Code", 16);
  font.setPixelSize(22);
  font.setItalic(false);
  font.setStyleHint(QFont::Monospace);
  font.setHintingPreference(QFont::PreferFullHinting);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
  painter.setFont(font);
  
  QColor baseColor = QColor(StyleManager::Colors::TERMINAL_CURSOR);
  
  int lineHeight = 28;
  int y = 30;
  
  for (int i = 0; i < logLines.size() && y < terminalImage.height() - 15; ++i) {
    QString line = logLines[i];
    
    if (line.length() > 120) {
      line = line.left(117) + "...";
    }
    
    painter.setPen(QColor(StyleManager::Colors::TERMINAL_CURSOR));
    painter.drawText(15, y, line);
    
    y += lineHeight;
  }
  
  // Scanline effect with semantic color
  painter.setPen(QColor(StyleManager::Colors::backgroundPrimaryAlpha(20)));
  for (int i = 0; i < terminalImage.height(); i += 2) {
    painter.drawLine(0, i, terminalImage.width(), i);
  }
  
  if (isValid()) {
    makeCurrent();
    if (terminalTexture) {
      terminalTexture->destroy();
      terminalTexture->create();
      terminalTexture->setData(terminalImage);
    }
    doneCurrent();
  }
}

void LoadingOverlay::appendLog(const QString &message)
{
  QMutexLocker locker(&logMutex);
  
  QStringList lines = message.split('\n');
  for (const QString &line : lines) {
    if (!line.trimmed().isEmpty()) {
      pendingLogLines.append(line);
    }
  }
  
  needsTextureUpdate = true;
}

void LoadingOverlay::initializeGL()
{
  initializeOpenGLFunctions();
  
  Logger::log(Logger::Info, QString("[LoadingOverlay] OpenGL version: %1").arg(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
  Logger::log(Logger::Info, QString("[LoadingOverlay] GLSL version: %1").arg(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
  
  if (windowHandle() && windowHandle()->screen()) {
    QScreen *screen = windowHandle()->screen();
    Logger::log(Logger::Info, QString("[LoadingOverlay] Screen: %1, DPR: %2, Physical DPI: %3x%4, Logical DPI: %5x%6")
                .arg(screen->name())
                .arg(screen->devicePixelRatio())
                .arg(screen->physicalDotsPerInchX())
                .arg(screen->physicalDotsPerInchY())
                .arg(screen->logicalDotsPerInchX())
                .arg(screen->logicalDotsPerInchY()));
  }
  
  timer.start();
  
  bool useSimpleShader = false;
  
  const char *vertexShaderSource;
  const char *fragmentShaderSource;
  
  if (useSimpleShader) {
    vertexShaderSource = R"(
      #version 120
      attribute vec2 aPos;
      void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
      }
    )";
    
    fragmentShaderSource = R"(
      #version 120
      uniform float time;
      uniform vec2 resolution;
      uniform sampler2D logoTexture;
      uniform sampler2D terminalTexture;
      
      void main() {
        vec2 uv = gl_FragCoord.xy / resolution.xy;
        vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
        
          vec3 col = vec3(0.0);
        float t = time * 0.5;
        col += vec3(0.1, 0.0, 0.1) * (0.5 + 0.5 * sin(t + p.x * 3.0));
        col += vec3(0.1, 0.0, 0.2) * (0.5 + 0.5 * cos(t * 0.7 + p.y * 2.0));
        float r = length(p);
        float a = atan(p.y, p.x);
        col += vec3(1.0, 0.5, 0.0) * 0.3 * sin(r * 10.0 - a * 3.0 - t) * exp(-r * 2.0);
        vec2 logoUV = (uv - 0.5) * 0.8 + 0.5;
        if(logoUV.x >= 0.0 && logoUV.x <= 1.0 && logoUV.y >= 0.0 && logoUV.y <= 1.0) {
          vec4 logoColor = texture2D(logoTexture, logoUV);
          float logoMask = (logoColor.a > 0.5 && length(logoColor.rgb) < 0.5) ? 1.0 : 0.0;
          col = mix(col, vec3(1.0) - col, logoMask);
        }
        vec2 terminalUV = uv;
        terminalUV = vec2(terminalUV.y, terminalUV.x);
        terminalUV = (terminalUV - vec2(0.5, 0.25)) * 1.25 + vec2(0.1, -0.3);
        terminalUV -= vec2(0.5, 0.5);
        vec2 rotated = vec2(terminalUV.y, -terminalUV.x);
        terminalUV = rotated + vec2(0.5, 0.5);
        
        if(terminalUV.x >= 0.0 && terminalUV.x <= 1.0 && terminalUV.y >= 0.0 && terminalUV.y <= 1.0) {
          vec4 terminalColor = texture2D(terminalTexture, terminalUV);
          float brightness = max(terminalColor.r, terminalColor.g);
          vec3 orange = vec3(1.0, 0.5, 0.0);
          vec3 phosphor = orange * brightness * 2.0;
          float terminalAlpha = brightness * 0.9;
          col = mix(col, col * 0.3 + phosphor, terminalAlpha);
        }
        
        gl_FragColor = vec4(col, 1.0);
      }
    )";
  } else {
#ifdef Q_OS_MAC
    vertexShaderSource = R"(
      #version 120
      attribute vec2 aPos;
      void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
      }
    )";
#else
    vertexShaderSource = R"(
      #version 120
      attribute vec2 aPos;
      void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
      }
    )";
#endif
    
    fragmentShaderSource = R"(
      #version 120
    uniform float time;
    uniform vec2 resolution;
    uniform sampler2D logoTexture;
    uniform sampler2D terminalTexture;
    uniform float terminalOffset;
    uniform float fadeToBlack;
    
    #define PI 3.14159265359
    #define TAU 6.28318530718
    
    mat2 rot(float a) {
      float s = sin(a), c = cos(a);
      return mat2(c, -s, s, c);
    }
    
    float noise(vec2 p) {
      return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
    }
    
    float smoothNoise(vec2 p) {
      vec2 i = floor(p);
      vec2 f = fract(p);
      
      float a = noise(i);
      float b = noise(i + vec2(1.0, 0.0));
      float c = noise(i + vec2(0.0, 1.0));
      float d = noise(i + vec2(1.0, 1.0));
      
      vec2 u = f * f * (3.0 - 2.0 * f);
      
      return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
    }
    
    vec3 turbulentSwirls(vec2 p) {
      vec3 col = vec3(0.0);
      
      float t = time * 0.1;
      
      for(float i = 0.0; i < 5.0; i++) {
        vec2 q = p;
        
        q *= 1.0 + i * 0.3;
        q = rot(t * (0.5 + i * 0.1)) * q;
        
        float n1 = smoothNoise(q * 3.0 + vec2(t, -t * 0.7));
        float n2 = smoothNoise(q * 5.0 - vec2(t * 1.3, t * 0.5));
        q += vec2(n1, n2) * 0.3;
        
        float r = length(q);
        float a = atan(q.y, q.x);
        float swirl = sin(r * 10.0 - a * 3.0 - t * 1.0);
        swirl *= exp(-r * 0.5);
        
        vec3 layerCol = mix(vec3(0.0, 0.2, 0.6), vec3(0.5, 0.0, 0.7), i / 4.0);
        col += layerCol * swirl * 0.1 / (1.0 + i);
      }
      
      return col;
    }
    
    vec3 particleField(vec2 p) {
      vec3 col = vec3(0.0);
      
      float t = time * 0.15;
      
      for(float i = 0.0; i < 15.0; i++) {
        vec2 pos = vec2(
          sin(i * 1.37 + t) * 0.8,
          cos(i * 1.73 + t * 0.7) * 0.8
        );
        
        float d = length(p - pos);
        float glow = exp(-d * d * 20.0);
        
        vec3 particleCol = mix(vec3(0.05, 0.4, 0.8), vec3(0.7, 0.1, 0.8), sin(i * 0.5 + t));
        col += particleCol * glow;
      }
      
      return col * 0.35;
    }
    
    vec3 cubeWireframe(vec2 p) {
      vec3 col = vec3(0.0);
      
      float t = time * 0.0625;
      vec3 angles = vec3(t, t * 0.7, t * 0.3);
      
      vec3 vertices[8];
      vertices[0] = vec3(-1.0, -1.0, -1.0);
      vertices[1] = vec3( 1.0, -1.0, -1.0);
      vertices[2] = vec3( 1.0,  1.0, -1.0);
      vertices[3] = vec3(-1.0,  1.0, -1.0);
      vertices[4] = vec3(-1.0, -1.0,  1.0);
      vertices[5] = vec3( 1.0, -1.0,  1.0);
      vertices[6] = vec3( 1.0,  1.0,  1.0);
      vertices[7] = vec3(-1.0,  1.0,  1.0);
      
      vec2 proj[8];
      for(int i = 0; i < 8; i++) {
        vec3 v = vertices[i];
        
        v.yz *= rot(angles.x);
        v.xz *= rot(angles.y);
        v.xy *= rot(angles.z);
        
        float scale = 2.0 / (4.0 + v.z);
        proj[i] = v.xy * scale * 0.3;
      }
      
      int edge0 = 0; int edge1 = 1;
      int edge2 = 1; int edge3 = 2;
      int edge4 = 2; int edge5 = 3;
      int edge6 = 3; int edge7 = 0;
      int edge8 = 4; int edge9 = 5;
      int edge10 = 5; int edge11 = 6;
      int edge12 = 6; int edge13 = 7;
      int edge14 = 7; int edge15 = 4;
      int edge16 = 0; int edge17 = 4;
      int edge18 = 1; int edge19 = 5;
      int edge20 = 2; int edge21 = 6;
      int edge22 = 3; int edge23 = 7;
      
      float minDist = 1e10;
      vec2 pa, ba;
      float h, d;
      pa = p - proj[0]; ba = proj[1] - proj[0];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[1]; ba = proj[2] - proj[1];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[2]; ba = proj[3] - proj[2];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[3]; ba = proj[0] - proj[3];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[4]; ba = proj[5] - proj[4];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[5]; ba = proj[6] - proj[5];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[6]; ba = proj[7] - proj[6];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[7]; ba = proj[4] - proj[7];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[0]; ba = proj[4] - proj[0];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[1]; ba = proj[5] - proj[1];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[2]; ba = proj[6] - proj[2];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      pa = p - proj[3]; ba = proj[7] - proj[3];
      h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
      minDist = min(minDist, length(pa - ba * h));
      
      col += vec3(1.0, 0.647, 0.0) * smoothstep(0.02, 0.0, minDist) * 2.0;
      
      return col;
    }
    
    vec3 flowField(vec2 p) {
      vec3 col = vec3(0.0);
      
      float t = time * 0.075;
      
      vec2 q = p * 4.0;
      
      for(float i = 0.0; i < 3.0; i++) {
        float n = smoothNoise(q + vec2(t, -t * 0.6) + i * 100.0);
        q += vec2(cos(n * TAU), sin(n * TAU)) * 0.3;
      }
      
      float flow = sin(q.x * 5.0 + q.y * 3.0 + t * 1.0);
      flow = smoothstep(0.0, 0.1, abs(flow));
      
      col += vec3(0.4, 0.05, 0.6) * (1.0 - flow) * 0.18;
      
      return col;
    }
    
    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
      
      vec3 col = vec3(0.0);
      
      col += turbulentSwirls(p) * 0.4;
      col += particleField(p) * 0.5;
      col += flowField(p) * 0.35;
      
      col += cubeWireframe(p) * 1.5;
      
      float vignette = 1.0 - length(p) * 0.6;
      col *= vignette;
      
      float logoSize = 0.8;
      vec2 logoP = p / logoSize;
      vec2 logoUV = logoP * 0.5 + 0.5;
      
      if(logoUV.x >= 0.0 && logoUV.x <= 1.0 && logoUV.y >= 0.0 && logoUV.y <= 1.0) {
        vec4 logoColor = texture2D(logoTexture, logoUV);
        float luminance = dot(logoColor.rgb, vec3(0.299, 0.587, 0.114));
        float logoMask = (logoColor.a > 0.5 && luminance < 0.5) ? 1.0 : 0.0;
        col = mix(col, 1.0 - col, logoMask);
      }
      float aspect = resolution.x / resolution.y;
      vec2 terminalUV = uv;
      terminalUV.x = terminalUV.x * aspect;
      terminalUV = vec2(terminalUV.y, terminalUV.x);
      terminalUV = (terminalUV - vec2(0.5, 0.5 * aspect)) * vec2(0.69, 1.04) + vec2(0.4, 0.5);
      terminalUV -= vec2(0.5, 0.5);
      vec2 rotated = vec2(terminalUV.y, -terminalUV.x);
      terminalUV = rotated + vec2(0.5, 0.5);
      
      if(terminalUV.x >= 0.0 && terminalUV.x <= 1.0 && terminalUV.y >= 0.0 && terminalUV.y <= 1.0) {
        vec2 scrolledUV = terminalUV + vec2(0.0, terminalOffset);
        vec4 terminalColor = texture2D(terminalTexture, scrolledUV);
        
        float brightness = max(terminalColor.r, terminalColor.g);
        vec3 orange = vec3(1.0, 0.5, 0.0);
        vec3 phosphor = orange * brightness * 1.5;
        
        vec2 texelSize = 1.0 / vec2(2048.0, 1789.0);
        float bloom = 0.0;
        for(int i = -2; i <= 2; i++) {
          for(int j = -2; j <= 2; j++) {
            vec2 offset = vec2(float(i), float(j)) * texelSize * 2.0;
            vec4 sample = texture2D(terminalTexture, scrolledUV + offset);
            float dist = length(vec2(float(i), float(j))) / 2.0;
            bloom += max(sample.r, sample.g) * exp(-dist * dist);
          }
        }
        bloom *= 0.05;
        
        phosphor += orange * bloom * 0.8;
        
        float terminalAlpha = brightness * 0.9;
        col = mix(col, col * 0.3 + phosphor, terminalAlpha);
      }
      
      col = col / (col + vec3(1.0));
      col = pow(col, vec3(0.4545));
      
      col = mix(col, vec3(0.0), fadeToBlack);
      
      gl_FragColor = vec4(col, 1.0);
    }
  )";
  }
  
  shaderProgram = new QOpenGLShaderProgram(this);
  if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Vertex shader compilation failed: %1").arg(shaderProgram->log()));
  }
  if (!shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Fragment shader compilation failed: %1").arg(shaderProgram->log()));
  }
  if (!shaderProgram->link()) {
    Logger::log(Logger::Error, QString("[LoadingOverlay] Shader linking failed: %1").arg(shaderProgram->log()));
  } else {
    Logger::log(Logger::Info, "[LoadingOverlay] Shader linked successfully");
  }
  
  timeUniform = shaderProgram->uniformLocation("time");
  resolutionUniform = shaderProgram->uniformLocation("resolution");
  logoTextureUniform = shaderProgram->uniformLocation("logoTexture");
  terminalTextureUniform = shaderProgram->uniformLocation("terminalTexture");
  terminalOffsetUniform = shaderProgram->uniformLocation("terminalOffset");
  fadeToBlackUniform = shaderProgram->uniformLocation("fadeToBlack");
  
  Logger::log(Logger::Info, QString("[LoadingOverlay] Shader uniforms - time: %1, resolution: %2, logoTexture: %3")
              .arg(timeUniform).arg(resolutionUniform).arg(logoTextureUniform));
  
  createLogoTexture();
  createTerminalTexture();
  
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void LoadingOverlay::resizeGL(int w, int h)
{
  qreal dpr = devicePixelRatio();
  int actualWidth = static_cast<int>(w * dpr);
  int actualHeight = static_cast<int>(h * dpr);
  glViewport(0, 0, actualWidth, actualHeight);
  
  Logger::log(Logger::Debug, QString("[LoadingOverlay] Resize: %1x%2 (DPR: %3, Actual: %4x%5)")
              .arg(w).arg(h).arg(dpr).arg(actualWidth).arg(actualHeight));
}

void LoadingOverlay::paintGL()
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  
  if (!shaderProgram || !logoTexture) {
    glClearColor(0.1f, 0.0f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Logger::log(Logger::Warning, "[LoadingOverlay] Shader or texture not ready");
    return;
  }
  
  if (!shaderProgram->isLinked()) {
    Logger::log(Logger::Error, "[LoadingOverlay] Shader program is not linked!");
    glClearColor(0.1f, 0.1f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }
  
  terminalScrollOffset += (targetScrollOffset - terminalScrollOffset) * 0.1f;
  
  if (!shaderProgram->bind()) {
    Logger::log(Logger::Error, "[LoadingOverlay] Failed to bind shader program!");
    return;
  }
  
  qreal dpr = devicePixelRatio();
  int actualWidth = static_cast<int>(width() * dpr);
  int actualHeight = static_cast<int>(height() * dpr);
  QSize fbSize = size() * dpr;
  
  shaderProgram->setUniformValue(timeUniform, timer.elapsed() / 1000.0f);
  shaderProgram->setUniformValue(resolutionUniform, QVector2D(fbSize.width(), fbSize.height()));
  shaderProgram->setUniformValue(logoTextureUniform, 0);
  shaderProgram->setUniformValue(terminalTextureUniform, 1);
  shaderProgram->setUniformValue(terminalOffsetUniform, terminalScrollOffset);
  shaderProgram->setUniformValue(fadeToBlackUniform, fadeToBlackValue);
  
  glActiveTexture(GL_TEXTURE0);
  logoTexture->bind();
  
  glActiveTexture(GL_TEXTURE1);
  if (terminalTexture) {
    terminalTexture->bind();
  }
  
  static const GLfloat vertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f
  };
  
  int vertexLocation = shaderProgram->attributeLocation("aPos");
  shaderProgram->enableAttributeArray(vertexLocation);
  shaderProgram->setAttributeArray(vertexLocation, vertices, 2);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  shaderProgram->disableAttributeArray(vertexLocation);
  
  shaderProgram->release();
}

bool LoadingOverlay::event(QEvent *event)
{
  if (event->type() == QEvent::ScreenChangeInternal) {
    Logger::log(Logger::Info, "[LoadingOverlay] Screen change detected, updating resolution");
    
    QSize currentSize = size();
    resizeGL(currentSize.width(), currentSize.height());
    update();
    
    return true;
  }
  
  return QOpenGLWidget::event(event);
}