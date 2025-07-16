#include "loadingoverlay.h"
#include <QTimer>
#include <QPainter>
#include <QImage>
#include <QFont>
#include <QFontMetrics>
#include <QMutexLocker>
#include <QTransform>
#include "../logger.h"

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
    
    // Clear log buffer
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
  
  QImage terminalImage(1024, 512, QImage::Format_ARGB32);
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
  
  QImage terminalImage(1024, 512, QImage::Format_ARGB32);
  terminalImage.fill(Qt::black);
  
  QPainter painter(&terminalImage);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::Antialiasing, true);
  
  QFont font("Cascadia Code", 16);
  font.setPixelSize(22);
  font.setBold(true);
  font.setItalic(true);
  font.setStyleHint(QFont::Monospace);
  font.setHintingPreference(QFont::PreferFullHinting);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 2.0);
  painter.setFont(font);
  
  QColor baseColor(255, 165, 0);
  
  int lineHeight = 28;
  int y = 30;
  
  for (int i = 0; i < logLines.size() && y < terminalImage.height() - 15; ++i) {
    QString line = logLines[i];
    
    if (line.length() > 60) {
      line = line.left(57) + "...";
    }
    
    painter.setPen(QColor(255, 165, 0));
    painter.drawText(15, y, line);
    
    y += lineHeight;
  }
  
  painter.setPen(QColor(0, 0, 0, 20));
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
  
  timer.start();
  
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
        
        vec3 layerCol = mix(vec3(1.0, 0.5, 0.0), vec3(0.8, 0.0, 0.4), i / 4.0);
        col += layerCol * swirl * 0.15 / (1.0 + i);
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
        
        vec3 particleCol = vec3(1.0, 0.3, 0.1);
        col += particleCol * glow;
      }
      
      return col * 0.5;
    }
    
    vec3 cubeWireframe(vec2 p) {
      vec3 col = vec3(0.0);
      
      float t = time * 0.0625;
      vec3 angles = vec3(t, t * 0.7, t * 0.3);
      
      vec3 vertices[8];
      vertices[0] = vec3(-1, -1, -1);
      vertices[1] = vec3( 1, -1, -1);
      vertices[2] = vec3( 1,  1, -1);
      vertices[3] = vec3(-1,  1, -1);
      vertices[4] = vec3(-1, -1,  1);
      vertices[5] = vec3( 1, -1,  1);
      vertices[6] = vec3( 1,  1,  1);
      vertices[7] = vec3(-1,  1,  1);
      
      vec2 proj[8];
      for(int i = 0; i < 8; i++) {
        vec3 v = vertices[i];
        
        v.yz *= rot(angles.x);
        v.xz *= rot(angles.y);
        v.xy *= rot(angles.z);
        
        float scale = 2.0 / (4.0 + v.z);
        proj[i] = v.xy * scale * 0.5;
      }
      
      int edges[24] = int[](
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
      );
      
      float minDist = 1e10;
      for(int i = 0; i < 12; i++) {
        vec2 a = proj[edges[i*2]];
        vec2 b = proj[edges[i*2+1]];
        
        vec2 pa = p - a;
        vec2 ba = b - a;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        float d = length(pa - ba * h);
        minDist = min(minDist, d);
      }
      
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
      
      col += vec3(0.8, 0.1, 0.5) * (1.0 - flow) * 0.2;
      
      return col;
    }
    
    void main() {
      vec2 uv = gl_FragCoord.xy / resolution.xy;
      vec2 p = (gl_FragCoord.xy - 0.5 * resolution.xy) / min(resolution.x, resolution.y);
      
      vec3 col = vec3(0.0);
      
      col += turbulentSwirls(p) * 0.7;
      col += particleField(p) * 0.8;
      col += flowField(p) * 0.5;
      
      col += cubeWireframe(p) * 1.5;
      
      float vignette = 1.0 - length(p) * 0.5;
      col *= vignette;
      
      vec2 logoUV = (uv - 0.5) * 0.8 + 0.5;
      
      if(logoUV.x >= 0.0 && logoUV.x <= 1.0 && logoUV.y >= 0.0 && logoUV.y <= 1.0) {
        vec4 logoColor = texture(logoTexture, logoUV);
        float luminance = dot(logoColor.rgb, vec3(0.299, 0.587, 0.114));
        float logoMask = (logoColor.a > 0.5 && luminance < 0.5) ? 1.0 : 0.0;
        col = mix(col, 1.0 - col, logoMask);
      }
      
      vec2 terminalUV = uv;
      
      terminalUV = vec2(terminalUV.y, terminalUV.x);
      
      terminalUV = (terminalUV - vec2(0.5, 0.25)) * 1.25 + vec2(0.1, -0.3);
      
      terminalUV -= vec2(0.5, 0.5);
      vec2 rotated = vec2(terminalUV.y, -terminalUV.x);
      terminalUV = rotated + vec2(0.5, 0.5);
      
      if(terminalUV.x >= 0.0 && terminalUV.x <= 1.0 && terminalUV.y >= 0.0 && terminalUV.y <= 1.0) {
        vec4 terminalColor = texture(terminalTexture, terminalUV);
        
        float brightness = max(terminalColor.r, terminalColor.g);
        vec3 orange = vec3(1.0, 0.5, 0.0);
        vec3 phosphor = orange * brightness * 1.5;
        
        vec2 texelSize = 1.0 / vec2(1024.0, 512.0);
        float bloom = 0.0;
        for(int i = -2; i <= 2; i++) {
          for(int j = -2; j <= 2; j++) {
            vec2 offset = vec2(float(i), float(j)) * texelSize * 2.0;
            vec4 sample = texture(terminalTexture, terminalUV + offset);
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
  glViewport(0, 0, w, h);
}

void LoadingOverlay::paintGL()
{
  glClear(GL_COLOR_BUFFER_BIT);
  
  if (!shaderProgram || !logoTexture) return;
  
  terminalScrollOffset += (targetScrollOffset - terminalScrollOffset) * 0.1f;
  
  shaderProgram->bind();
  
  shaderProgram->setUniformValue(timeUniform, timer.elapsed() / 1000.0f);
  shaderProgram->setUniformValue(resolutionUniform, QVector2D(width(), height()));
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