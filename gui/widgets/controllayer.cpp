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

void CircularButton::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QPushButton::enterEvent(event);
}

void CircularButton::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QPushButton::leaveEvent(event);
}

void CircularButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Create a circular path for the button
    QPainterPath circlePath;
    QRect buttonRect = rect().adjusted(1, 1, -1, -1);  // Slight inset for border
    circlePath.addEllipse(buttonRect);

    // Draw the button background gradient
    QLinearGradient gradient(buttonRect.topLeft(), buttonRect.bottomLeft());

    if (m_hovered) {
        // Brighter gradient on hover
        gradient.setColorAt(0, QColor(255, 255, 255, 130));
        gradient.setColorAt(0.5, QColor(255, 255, 255, 100));
        gradient.setColorAt(1, QColor(255, 255, 255, 120));
    } else {
        // Normal gradient
        gradient.setColorAt(0, QColor(255, 255, 255, 100));
        gradient.setColorAt(0.5, QColor(255, 255, 255, 70));
        gradient.setColorAt(1, QColor(255, 255, 255, 90));
    }

    painter.fillPath(circlePath, gradient);

    // Draw border - slightly brighter on hover
    if (m_hovered) {
        painter.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
    } else {
        painter.setPen(QPen(QColor(255, 255, 255, 180), 1));
    }
    painter.drawEllipse(buttonRect);

    // Now create inverted icon using screen capture
    if (!icon().isNull()) {
        // For simplicity, just use cutout with inverted fill in the icon area
        QPixmap iconPixmap = icon().pixmap(iconSize());

        // Create the button with inverted icon
        QPixmap finalButton(size());
        finalButton.fill(Qt::transparent);

        QPainter finalPainter(&finalButton);
        finalPainter.setRenderHint(QPainter::Antialiasing);

        // Draw button background
        finalPainter.fillPath(circlePath, gradient);
        finalPainter.setPen(QPen(QColor(255, 255, 255, 180), 1));
        finalPainter.drawEllipse(buttonRect);

        // Draw the icon in inverted colors (white where normally black)
        QRect iconRect = QRect(0, 0, iconSize().width(), iconSize().height());
        iconRect.moveCenter(rect().center());

        // Draw the icon directly (it's already black in the SVG)
        finalPainter.drawPixmap(iconRect, iconPixmap);

        finalPainter.end();
        painter.drawPixmap(0, 0, finalButton);
    }
}

ControlLayer::ControlLayer(QWidget *parent)
    : QWidget(parent), m_consoleVisible(false)
{
  setAttribute(Qt::WA_TranslucentBackground);
  setupControls();
  connectSignals();
}

void ControlLayer::setupControls()
{
  m_sizeDownButton = new CircularButton("", this);  // Will use icon
  m_sizeDownButton->setIcon(QIcon(":/images/nav-controls/minus.svg"));
  m_sizeDownButton->setIconSize(QSize(13, 13));

  m_sizeUpButton = new CircularButton("", this);  // Will use icon
  m_sizeUpButton->setIcon(QIcon(":/images/nav-controls/plus.svg"));
  m_sizeUpButton->setIconSize(QSize(13, 13));

  m_openExternalBrowserButton = new CircularButton("", this);  // Will use icon
  m_openExternalBrowserButton->setIcon(QIcon(":/images/nav-controls/external-link.svg"));
  m_openExternalBrowserButton->setIconSize(QSize(13, 13));

  m_resetBrowserButton = new CircularButton("", this);  // Will use icon
  m_resetBrowserButton->setIcon(QIcon(":/images/nav-controls/refresh.svg"));
  m_resetBrowserButton->setIconSize(QSize(13, 13));

  m_consoleToggleButton = new CircularButton("", this);  // Will use icon
  m_consoleToggleButton->setIcon(QIcon(":/images/nav-controls/chevron-up.svg"));
  m_consoleToggleButton->setIconSize(QSize(13, 13));
  m_consoleToggleButton->setToolTip("Toggle Debug Pane");

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

  m_sizeDownButton->setFixedSize(30, 30);
  m_sizeUpButton->setFixedSize(30, 30);
  m_openExternalBrowserButton->setFixedSize(30, 30);
  m_resetBrowserButton->setFixedSize(30, 30);
  m_consoleToggleButton->setFixedSize(30, 30);

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