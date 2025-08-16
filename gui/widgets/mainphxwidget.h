#ifndef MAINPHXWIDGET_H
#define MAINPHXWIDGET_H

#include "phxwidget.h"

// Constants for timing
constexpr int DEFAULT_FADE_DURATION_MS = 1000;

class MainPhxWidget : public PhxWidget
{
  Q_OBJECT

public:
  explicit MainPhxWidget(bool devMode, QWidget *parent = nullptr);
  
  // Load the shader page for boot sequence
  void loadShaderPage();
  
  // Fade out the shader over specified duration
  void fadeShader(int durationMs = DEFAULT_FADE_DURATION_MS);
  
  // Transition from shader to main app
  void transitionToApp(const QUrl &url);
};

#endif // MAINPHXWIDGET_H