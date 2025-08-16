#include "controllayer.h"
#include "StyleManager.h"
#include <QResizeEvent>

ControlLayer::ControlLayer(QWidget *parent)
    : QWidget(parent), m_consoleVisible(false)
{
  setAttribute(Qt::WA_TranslucentBackground);
  setupControls();
  connectSignals();
}

void ControlLayer::setupControls()
{
  m_sizeDownButton = new QPushButton("−", this);  // Unicode minus sign for zoom out
  m_sizeUpButton = new QPushButton("+", this);  // Plus for zoom in
  m_openExternalBrowserButton = new QPushButton("λ", this);  // Lambda symbol for External
  m_resetBrowserButton = new QPushButton("τ", this);  // Tau symbol for Reset
  m_consoleToggleButton = new QPushButton("▲", this);  // Up arrow for debug pane
  m_consoleToggleButton->setToolTip("Toggle Debug Pane");
  
  // Add tooltips for all buttons
  m_sizeDownButton->setToolTip("Zoom Out");
  m_sizeUpButton->setToolTip("Zoom In");
  m_openExternalBrowserButton->setToolTip("Open in External Browser (Lambda)");
  m_resetBrowserButton->setToolTip("Reset Browser (Tau)");

  QString buttonStyle = StyleManager::invertedButton();

  m_sizeDownButton->setStyleSheet(buttonStyle);
  m_sizeUpButton->setStyleSheet(buttonStyle);
  m_openExternalBrowserButton->setStyleSheet(buttonStyle);
  m_resetBrowserButton->setStyleSheet(buttonStyle);
  m_consoleToggleButton->setStyleSheet(buttonStyle);

  m_sizeDownButton->setFixedWidth(30);
  m_sizeUpButton->setFixedWidth(30);
  m_openExternalBrowserButton->setFixedWidth(30);
  m_resetBrowserButton->setFixedWidth(30);
  m_consoleToggleButton->setFixedWidth(30);

  m_buttonLayout = new QHBoxLayout();
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->addWidget(m_consoleToggleButton);
  m_buttonLayout->addWidget(m_resetBrowserButton);
  m_buttonLayout->addWidget(m_openExternalBrowserButton);
  m_buttonLayout->addWidget(m_sizeDownButton);
  m_buttonLayout->addWidget(m_sizeUpButton);

  setLayout(m_buttonLayout);
  setStyleSheet(
      QString("ControlLayer { "
              "  background-color: rgba(0, 0, 0, 100); "  // Semi-transparent black background
              "  border-top: 1px solid rgba(255, 255, 255, 30); "  // Semi-transparent white border
              "  border-bottom: 1px solid rgba(255, 255, 255, 30); "
              "}"));

  positionControls();
}

void ControlLayer::connectSignals()
{
  connect(m_sizeDownButton, &QPushButton::released, this, &ControlLayer::sizeDown);
  connect(m_sizeUpButton, &QPushButton::released, this, &ControlLayer::sizeUp);
  connect(m_openExternalBrowserButton, &QPushButton::released, this, &ControlLayer::openExternalBrowser);
  connect(m_resetBrowserButton, &QPushButton::released, this, &ControlLayer::resetBrowser);
  connect(m_consoleToggleButton, &QPushButton::released, this, &ControlLayer::toggleConsole);
}

void ControlLayer::setConsoleVisible(bool visible)
{
  m_consoleVisible = visible;
  if (m_consoleVisible)
  {
    m_consoleToggleButton->setText("▼");  // Down arrow when console is visible (can hide)
  }
  else
  {
    m_consoleToggleButton->setText("▲");  // Up arrow when console is hidden (can show)
  }
}

void ControlLayer::setDebugPaneAvailable(bool available)
{
  m_consoleToggleButton->setVisible(available);
  if (!available && m_buttonLayout) {
    // Adjust layout to remove empty space
    m_buttonLayout->invalidate();
    m_buttonLayout->update();
    adjustSize();
    positionControls();
  }
}

void ControlLayer::positionControls()
{
  QWidget *parentWidget = this->parentWidget();
  if (!parentWidget)
    return;

  QSize containerSize = this->sizeHint();
  int margin = 10;
  int scrollbarBuffer = 30;

  int xPos = parentWidget->width() - containerSize.width() - margin - scrollbarBuffer;
  int yPos = parentWidget->height() - containerSize.height() - margin;

  setGeometry(xPos, yPos, containerSize.width(), containerSize.height());
  raise();
}

void ControlLayer::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  positionControls();
}