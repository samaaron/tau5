#include "mainphxwidget.h"
#include "shaderpage.h"
#include "../shared/tau5logger.h"
#include <QTimer>
#include <QVariant>

MainPhxWidget::MainPhxWidget(bool devMode, QWidget *parent)
    : PhxWidget(devMode, parent)
{
}

void MainPhxWidget::loadShaderPage()
{
  Tau5Logger::instance().info( "[PHX] Loading shader page");
  
  getWebView()->setHtml(ShaderPage::getHtml(), QUrl("qrc:/"));
  getWebView()->show();
  setPhxAlive(false);
  
  connect(getWebView(), &PhxWebView::loadFinished, this, [this](bool ok) {
    if (ok) {
      getWebView()->page()->runJavaScript(R"(
        if (!window.webGLFailed) {
          window.dispatchEvent(new Event('resize'));
        }
        window.webGLFailed ? 'failed' : 'success';
      )", [this](const QVariant &result) {
        QString status = result.toString();
        if (status == "failed") {
          Tau5Logger::instance().warning( "[PHX] WebGL not supported, using fallback");
        } else {
          Tau5Logger::instance().info( "[PHX] Shader page loaded with WebGL");
        }
        emit pageLoaded();
      });
    }
  }, Qt::SingleShotConnection);
}

void MainPhxWidget::fadeShader(int durationMs)
{
  Tau5Logger::instance().info( QString("[PHX] Fading shader over %1ms").arg(durationMs));
  
  QString fadeScript = QString(R"(
    (function() {
      const startTime = Date.now();
      const duration = %1;
      
      function updateFade() {
        const elapsed = Date.now() - startTime;
        const progress = Math.min(elapsed / duration, 1.0);
        
        if (window.gl && window.fadeUniform !== undefined) {
          window.gl.uniform1f(window.fadeUniform, progress);
        }
        
        const canvas = document.getElementById('canvas');
        if (canvas) {
          canvas.style.opacity = 1.0 - progress;
        }
        
        if (progress < 1.0) {
          requestAnimationFrame(updateFade);
        }
      }
      
      updateFade();
    })();
  )").arg(durationMs);
  
  getWebView()->page()->runJavaScript(fadeScript);
}

void MainPhxWidget::transitionToApp(const QUrl &url)
{
  Tau5Logger::instance().info( QString("[PHX] Transitioning to app at: %1").arg(url.toString()));
  
  // Just load the new page directly - the overlay covers the transition
  connectToTauPhx(url);
}