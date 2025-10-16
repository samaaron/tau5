#include "controllayer.h"
#include "StyleManager.h"
#include <QApplication>
#include <QResizeEvent>
#include <QIcon>
#include <QPainter>
#include <QStyleOptionButton>
#include <QSvgRenderer>
#include <QPainterPath>
#include <QScreen>
#include <QImage>
#include <cmath>

void CircularButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    m_animationTimer->start();
    QPushButton::enterEvent(event);
}

void CircularButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    m_animationTimer->start();
    QPushButton::leaveEvent(event);
}

void CircularButton::updateAnimation()
{
    const qreal transitionSpeed = 0.20; // Transition speed for hover state change

    if (m_hovered) {
        // Transition to fully hovered state
        if (m_hoverProgress < 1.0) {
            m_hoverProgress += transitionSpeed;
            if (m_hoverProgress >= 1.0) {
                m_hoverProgress = 1.0;
                m_animationTimer->stop();
            }
            update();
        }
    } else {
        // Transition back to normal state
        if (m_hoverProgress > 0.0) {
            m_hoverProgress -= transitionSpeed;
            if (m_hoverProgress <= 0.0) {
                m_hoverProgress = 0.0;
                m_animationTimer->stop();
            }
            update();
        }
    }
}

void CircularButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Create a circular path for the button
    // Add more inset to prevent clipping of outer border
    QPainterPath circlePath;
    QRect buttonRect = rect().adjusted(3, 3, -3, -3);
    circlePath.addEllipse(buttonRect);

    // Interpolation helper (linear)
    auto lerp = [](qreal start, qreal end, qreal progress) {
        return start + (end - start) * progress;
    };

    // Standard house orange color (from StyleManager)
    int orangeR = 255;
    int orangeG = 165;
    int orangeB = 0;

    // FLAT SOLID DESIGN - interpolate from transparent white to solid orange
    int alpha = lerp(100, 255, m_hoverProgress);
    int r = lerp(255, orangeR, m_hoverProgress);
    int g = lerp(255, orangeG, m_hoverProgress);
    int b = lerp(255, orangeB, m_hoverProgress);

    // Draw flat solid background - no gradients
    painter.fillPath(circlePath, QColor(r, g, b, alpha));

    // Inner border - matches background color so it blends in
    painter.setPen(QPen(QColor(r, g, b, alpha), 1.5));
    painter.drawEllipse(buttonRect);

    // Outer border - same width on hover and non-hover, transitions from black to white
    QRect outerRect = buttonRect.adjusted(-2, -2, 2, 2);
    int outerBorderR = lerp(0, 255, m_hoverProgress);
    int outerBorderG = lerp(0, 255, m_hoverProgress);
    int outerBorderB = lerp(0, 255, m_hoverProgress);
    int outerBorderAlpha = 200;  // Constant alpha - no change on hover

    painter.setPen(QPen(QColor(outerBorderR, outerBorderG, outerBorderB, outerBorderAlpha), 1.5));
    painter.drawEllipse(outerRect);

    if (!icon().isNull()) {
        // Calculate icon position
        QRect iconRect = QRect(0, 0, iconSize().width(), iconSize().height());
        iconRect.moveCenter(rect().center());

        // Draw icon with color interpolation (black -> white on hover)
        if (m_hoverProgress > 0.01) {
            // Create a color filter that interpolates from black to white
            QImage iconImage = icon().pixmap(iconSize()).toImage();

            // Apply color transformation
            for (int y = 0; y < iconImage.height(); y++) {
                for (int x = 0; x < iconImage.width(); x++) {
                    QColor pixelColor = iconImage.pixelColor(x, y);
                    int alpha = pixelColor.alpha();

                    if (alpha > 0) {
                        // Interpolate from black (0,0,0) to white (255,255,255)
                        int newValue = lerp(0, 255, m_hoverProgress);
                        iconImage.setPixelColor(x, y, QColor(newValue, newValue, newValue, alpha));
                    }
                }
            }

            painter.drawImage(iconRect, iconImage);
        } else {
            // Draw the icon directly when not hovering - QIcon handles DPI scaling internally
            icon().paint(&painter, iconRect);
        }
    }
}

ControlLayer::ControlLayer(QWidget *parent)
    : QWidget(parent), m_consoleVisible(false)
{
  setAttribute(Qt::WA_TranslucentBackground);
  setupControls();
  connectSignals();

  // Install event filter on parent to track resize events
  if (parent) {
    parent->installEventFilter(this);
  }
}

void ControlLayer::setupControls()
{
  // Fixed icon size for consistent appearance across all platforms
  // 16x16 logical pixels - will automatically scale with DPI
  QSize iconSize(16, 16);

  m_sizeDownButton = new CircularButton("", this);  // Will use icon
  m_sizeDownButton->setIcon(QIcon(":/images/nav-controls/minus.svg"));
  m_sizeDownButton->setIconSize(iconSize);

  m_sizeUpButton = new CircularButton("", this);  // Will use icon
  m_sizeUpButton->setIcon(QIcon(":/images/nav-controls/plus.svg"));
  m_sizeUpButton->setIconSize(iconSize);

  m_openExternalBrowserButton = new CircularButton("", this);  // Will use icon
  m_openExternalBrowserButton->setIcon(QIcon(":/images/nav-controls/external-link.svg"));
  m_openExternalBrowserButton->setIconSize(iconSize);

  m_resetBrowserButton = new CircularButton("", this);  // Will use icon
  m_resetBrowserButton->setIcon(QIcon(":/images/nav-controls/refresh.svg"));
  m_resetBrowserButton->setIconSize(iconSize);

  m_consoleToggleButton = new CircularButton("", this);  // Will use icon
  m_consoleToggleButton->setIcon(QIcon(":/images/nav-controls/chevron-up.svg"));
  m_consoleToggleButton->setIconSize(iconSize);
  m_consoleToggleButton->setToolTip("Toggle Debug Pane");

  m_saveImageButton = new CircularButton("", this);  // Will use icon
  m_saveImageButton->setIcon(QIcon(":/images/nav-controls/image.svg"));
  m_saveImageButton->setIconSize(iconSize);
  m_saveImageButton->setToolTip("Save as Image");

  // Add tooltips for all buttons
  m_sizeDownButton->setToolTip("Zoom Out");
  m_sizeUpButton->setToolTip("Zoom In");
  m_openExternalBrowserButton->setToolTip("Open in External Browser");
  m_resetBrowserButton->setToolTip("Reset Browser");

  // Create a circular button style for SVG icons
  QString circularButtonStyle = QString(
      "QPushButton { "
      "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
      "    stop:0 rgba(255, 255, 255, 100), "
      "    stop:0.5 rgba(255, 255, 255, 70), "
      "    stop:1 rgba(255, 255, 255, 90)); "  // Glass-like gradient
      "  color: rgb(0, 0, 0); "  // Black color for SVG currentColor
      "  border: 1px solid rgba(255, 255, 255, 180); " // Bright border
      "  padding: 5px; "
      "  border-radius: 15px; "  // Half of 30px width/height for perfect circle
      "}"
      "QPushButton:hover { "
      "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
      "    stop:0 rgba(255, 255, 255, 120), "
      "    stop:0.5 rgba(255, 255, 255, 90), "
      "    stop:1 rgba(255, 255, 255, 110)); "  // Brighter gradient on hover
      "  border: 1px solid rgba(255, 255, 255, 220); "
      "  color: rgb(0, 0, 0); "  // Keep black on hover
      "}"
      "QPushButton:pressed { "
      "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
      "    stop:0 rgba(255, 255, 255, 90), "
      "    stop:0.5 rgba(255, 255, 255, 110), "
      "    stop:1 rgba(255, 255, 255, 130)); "  // Inverted gradient when pressed
      "  border: 1px solid rgba(255, 255, 255, 255); "
      "  color: rgb(0, 0, 0); "  // Keep black when pressed
      "}");

  m_sizeDownButton->setStyleSheet(circularButtonStyle);
  m_sizeUpButton->setStyleSheet(circularButtonStyle);
  m_openExternalBrowserButton->setStyleSheet(circularButtonStyle);
  m_resetBrowserButton->setStyleSheet(circularButtonStyle);
  m_consoleToggleButton->setStyleSheet(circularButtonStyle);
  m_saveImageButton->setStyleSheet(circularButtonStyle);

  m_sizeDownButton->setFixedSize(30, 30);
  m_sizeUpButton->setFixedSize(30, 30);
  m_openExternalBrowserButton->setFixedSize(30, 30);
  m_resetBrowserButton->setFixedSize(30, 30);
  m_consoleToggleButton->setFixedSize(30, 30);
  m_saveImageButton->setFixedSize(30, 30);

  m_buttonLayout = new QHBoxLayout();
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(5);  // Fixed spacing for consistent appearance across platforms
  m_buttonLayout->addWidget(m_consoleToggleButton);
  m_buttonLayout->addWidget(m_resetBrowserButton);
  m_buttonLayout->addWidget(m_saveImageButton);
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
  connect(m_saveImageButton, &QPushButton::released, this, &ControlLayer::saveAsImage);
}

void ControlLayer::setConsoleVisible(bool visible)
{
  m_consoleVisible = visible;
  if (m_consoleVisible)
  {
    m_consoleToggleButton->setIcon(QIcon(":/images/nav-controls/chevron-down.svg"));  // Down arrow when console is visible
  }
  else
  {
    m_consoleToggleButton->setIcon(QIcon(":/images/nav-controls/chevron-up.svg"));  // Up arrow when console is hidden
  }
}

void ControlLayer::setDebugPaneAvailable(bool available)
{
  // Keep the button always visible, just disable it if debug pane is not available
  m_consoleToggleButton->setEnabled(available);
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

bool ControlLayer::eventFilter(QObject *obj, QEvent *event)
{
  // Watch for parent resize events to reposition controls
  if (obj == parentWidget() && event->type() == QEvent::Resize) {
    positionControls();
  }
  return QWidget::eventFilter(obj, event);
}