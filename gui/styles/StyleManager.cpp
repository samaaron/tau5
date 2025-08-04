#include "StyleManager.h"

// Primary brand colors
const QString StyleManager::Colors::ACCENT_PRIMARY = "#ffa500";
const QString StyleManager::Colors::ACCENT_PRIMARY_RGB = "rgb(255, 165, 0)";
const QString StyleManager::Colors::ACCENT_SECONDARY = "rgb(255, 20, 147)";
const QString StyleManager::Colors::ACCENT_HIGHLIGHT = "#1e90ff";

// Background colors
const QString StyleManager::Colors::BACKGROUND_PRIMARY = "#000000";
const QString StyleManager::Colors::BACKGROUND_SECONDARY = "#1e1e1e";
const QString StyleManager::Colors::BACKGROUND_TERTIARY = "#1a1a1a";
const QString StyleManager::Colors::BACKGROUND_CONSOLE = "#000000";
const QString StyleManager::Colors::BACKGROUND_SURFACE = "#0a0a0a";
const QString StyleManager::Colors::BACKGROUND_SURFACE_LIGHT = "#333333";

// Text colors
const QString StyleManager::Colors::TEXT_PRIMARY = "#ffffff";
const QString StyleManager::Colors::TEXT_SECONDARY = "#e0e0e0";
const QString StyleManager::Colors::TEXT_TERTIARY = "#b0b0b0";
const QString StyleManager::Colors::TEXT_MUTED = "#888888";
const QString StyleManager::Colors::TEXT_TIMESTAMP = "#888888";

// Interactive element colors
const QString StyleManager::Colors::SCROLLBAR_TRACK = "#1e1e1e";
const QString StyleManager::Colors::SCROLLBAR_THUMB = "#5e5e5e";
const QString StyleManager::Colors::SCROLLBAR_THUMB_HOVER = "#888888";
const QString StyleManager::Colors::BUTTON_HOVER = "#ff8c00";
const QString StyleManager::Colors::BUTTON_ACTIVE = "#cc6600";

// Special purpose colors
const QString StyleManager::Colors::TERMINAL_CURSOR = "rgb(255, 20, 147)";
const QString StyleManager::Colors::SELECTION_BACKGROUND = "rgb(255, 20, 147)";
const QString StyleManager::Colors::SELECTION_TEXT = "#000000";
const QString StyleManager::Colors::BORDER_DEFAULT = "#ffa500";
const QString StyleManager::Colors::SCANLINE_OVERLAY = "rgba(0, 0, 0, 0.08)";

// Status colors
const QString StyleManager::Colors::STATUS_ERROR = "#4169e1";
const QString StyleManager::Colors::STATUS_WARNING = "#ff6600";
const QString StyleManager::Colors::STATUS_SUCCESS = "#00ff00";
const QString StyleManager::Colors::STATUS_INFO = "#1e90ff";

// Legacy color name mappings
const QString& StyleManager::Colors::PRIMARY_ORANGE = ACCENT_PRIMARY;
const QString& StyleManager::Colors::PRIMARY_ORANGE_RGB = ACCENT_PRIMARY_RGB;
const QString& StyleManager::Colors::ERROR_BLUE = STATUS_ERROR;
const QString& StyleManager::Colors::TIMESTAMP_GRAY = TEXT_TIMESTAMP;
const QString& StyleManager::Colors::BLACK = BACKGROUND_PRIMARY;
const QString& StyleManager::Colors::WHITE = TEXT_PRIMARY;
const QString& StyleManager::Colors::DEEP_PINK = ACCENT_SECONDARY;
const QString& StyleManager::Colors::DARK_BACKGROUND = BACKGROUND_SECONDARY;
const QString& StyleManager::Colors::CONSOLE_BACKGROUND = BACKGROUND_CONSOLE;

// Alpha variants implementation
QString StyleManager::Colors::accentPrimaryAlpha(int alpha)
{
  return QString("rgba(255, 165, 0, %1)").arg(alpha);
}

QString StyleManager::Colors::backgroundPrimaryAlpha(int alpha)
{
  return QString("rgba(0, 0, 0, %1)").arg(alpha);
}

QString StyleManager::Colors::textPrimaryAlpha(int alpha)
{
  return QString("rgba(255, 255, 255, %1)").arg(alpha);
}

QString StyleManager::Colors::statusErrorAlpha(int alpha)
{
  return QString("rgba(65, 105, 225, %1)").arg(alpha);
}

// Alpha conversion helpers - takes decimal 0.0-1.0
QString StyleManager::Colors::accentPrimaryAlpha(double alpha)
{
  return QString("rgba(255, 165, 0, %1)").arg(alpha);
}

QString StyleManager::Colors::backgroundPrimaryAlpha(double alpha)
{
  return QString("rgba(0, 0, 0, %1)").arg(alpha);
}

QString StyleManager::Colors::statusErrorAlpha(double alpha)
{
  return QString("rgba(65, 105, 225, %1)").arg(alpha);
}

// Typography Definitions
const QString StyleManager::Typography::MONOSPACE_FONT_FAMILY = "'Consolas', 'Monaco', 'Courier New', monospace";
const QString StyleManager::Typography::DEFAULT_FONT_FAMILY = "system-ui, sans-serif";

const QString StyleManager::Typography::FONT_SIZE_SMALL = "10px";
const QString StyleManager::Typography::FONT_SIZE_MEDIUM = "12px";
const QString StyleManager::Typography::FONT_SIZE_LARGE = "14px";

const QString StyleManager::Typography::FONT_WEIGHT_NORMAL = "normal";
const QString StyleManager::Typography::FONT_WEIGHT_BOLD = "bold";

// Spacing Definitions
const QString StyleManager::Spacing::EXTRA_SMALL = "2px";
const QString StyleManager::Spacing::SMALL = "4px";
const QString StyleManager::Spacing::MEDIUM = "8px";
const QString StyleManager::Spacing::LARGE = "12px";
const QString StyleManager::Spacing::EXTRA_LARGE = "16px";

// Common Style Components
QString StyleManager::darkGradientBackground()
{
  return QString(
             "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
             "  stop:0 %1, "
             "  stop:0.3 %2, "
             "  stop:0.7 %2, "
             "  stop:1 %3);")
      .arg(Colors::blackAlpha(191))
      .arg(Colors::blackAlpha(191))
      .arg(Colors::blackAlpha(191));
}

QString StyleManager::headerGradientBackground()
{
  return QString(
             "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
             "  stop:0 rgba(26, 26, 26, 191), "
             "  stop:0.5 rgba(15, 15, 15, 191), "
             "  stop:1 %1);")
      .arg(Colors::blackAlpha(191));
}

QString StyleManager::primaryButton()
{
  return QString(
             "QPushButton { "
             "  background-color: %1; "
             "  color: %2; "
             "  border: 1px solid %2; "
             "  font-family: %3; "
             "  font-weight: %4; "
             "  padding: %5 %6; "
             "  border-radius: %7; "
             "}"
             "QPushButton:hover { "
             "  background-color: %8; "
             "}"
             "QPushButton:pressed { "
             "  background-color: %9; "
             "}")
      .arg(Colors::PRIMARY_ORANGE)
      .arg(Colors::BLACK)
      .arg(Typography::MONOSPACE_FONT_FAMILY)
      .arg(Typography::FONT_WEIGHT_BOLD)
      .arg(Spacing::SMALL)
      .arg(Spacing::MEDIUM)
      .arg(Spacing::EXTRA_SMALL)
      .arg(Colors::primaryOrangeAlpha(220))
      .arg(Colors::primaryOrangeAlpha(180));
}

QString StyleManager::tau5Scrollbar()
{
  return QString(
             "QScrollBar:vertical { "
             "  background: transparent; "
             "  width: 8px; "
             "  border: none; "
             "  margin: 0px; "
             "}"
             "QScrollBar::handle:vertical { "
             "  background: %1; "
             "  border-radius: 0px; "
             "  min-height: 30px; "
             "  margin: 0px; "
             "  border: none; "
             "}"
             "QScrollBar::handle:vertical:hover { "
             "  background: %2; "
             "}"
             "QScrollBar::handle:vertical:pressed { "
             "  background: %2; "
             "}"
             "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
             "  height: 0px; "
             "  background: transparent; "
             "  border: none; "
             "}"
             "QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical { "
             "  background: transparent; "
             "  border: none; "
             "}"
             "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { "
             "  background: transparent; "
             "  border: none; "
             "}")
      .arg(Colors::primaryOrangeAlpha(240))
      .arg(Colors::primaryOrangeAlpha(255));
}

QString StyleManager::orangeBorder(const QString &width)
{
  return QString("border: %1 solid %2;")
      .arg(width)
      .arg(Colors::primaryOrangeAlpha(150));
}

QString StyleManager::textEdit()
{
  return QString(
             "QTextEdit { "
             "  %1 "
             "  color: %2; "
             "  font-family: %3; "
             "  font-size: %4; "
             "  border: none; "
             "  padding: %5; "
             "  selection-background-color: %6; "
             "  selection-color: %7; "
             "}"
             "QMenu { "
             "  background-color: %8; "
             "  color: %9; "
             "  border: 1px solid %10; "
             "  padding: 4px; "
             "}"
             "QMenu::item { "
             "  padding: 4px 20px; "
             "  background-color: transparent; "
             "}"
             "QMenu::item:selected { "
             "  background-color: %11; "
             "  color: %12; "
             "}"
             "QMenu::separator { "
             "  height: 1px; "
             "  background-color: %13; "
             "  margin: 4px 10px; "
             "}")
      .arg(darkGradientBackground())
      .arg(Colors::PRIMARY_ORANGE)
      .arg(Typography::MONOSPACE_FONT_FAMILY)
      .arg(Typography::FONT_SIZE_MEDIUM)
      .arg(Spacing::LARGE)
      .arg(Colors::DEEP_PINK)
      .arg(Colors::BLACK)
      .arg(Colors::blackAlpha(240))
      .arg(Colors::PRIMARY_ORANGE)
      .arg(Colors::primaryOrangeAlpha(100))
      .arg(Colors::primaryOrangeAlpha(40))
      .arg(Colors::PRIMARY_ORANGE)
      .arg(Colors::primaryOrangeAlpha(60));
}

QString StyleManager::checkbox()
{
  return QString(
             "QCheckBox { "
             "  background: transparent; "
             "  color: %1; "
             "  font-family: %2; "
             "  font-size: %3; "
             "  font-weight: %4; "
             "  spacing: %5; "
             "}"
             "QCheckBox::indicator { "
             "  width: 16px; "
             "  height: 16px; "
             "  border-radius: 3px; "
             "  background: %6; "
             "  border: 2px solid %7; "
             "}"
             "QCheckBox::indicator:checked { "
             "  background: %8; "
             "  border: 2px solid %9; "
             "}"
             "QCheckBox::indicator:hover { "
             "  border: 2px solid %10; "
             "}")
      .arg(Colors::PRIMARY_ORANGE)
      .arg(Typography::MONOSPACE_FONT_FAMILY)
      .arg(Typography::FONT_SIZE_SMALL)
      .arg(Typography::FONT_WEIGHT_BOLD)
      .arg(Spacing::SMALL)
      .arg(Colors::blackAlpha(150))
      .arg(Colors::primaryOrangeAlpha(150))
      .arg(Colors::primaryOrangeAlpha(200))
      .arg(Colors::primaryOrangeAlpha(255))
      .arg(Colors::primaryOrangeAlpha(255));
}

// Component-specific styles
QString StyleManager::consoleHeader()
{
  return QString(
             "QWidget { "
             "  %1 "
             "  padding-top: 6px; "
             "}")
      .arg(headerGradientBackground());
}

QString StyleManager::consoleOutput()
{
  return textEdit() + tau5Scrollbar();
}

QString StyleManager::guiButton()
{
  return primaryButton();
}

QString StyleManager::mainWindow()
{
  return QString("background-color: %1;").arg(Colors::BLACK);
}

QString StyleManager::contextMenu()
{
  return QString(
    "QMenu {"
    "  background-color: %1;"
    "  border: 1px solid %2;"
    "  padding: %3;"
    "}"
    "QMenu::item {"
    "  padding: %4 %5;"
    "  padding-left: %11;"  // Extra padding for icon space
    "  background-color: transparent;"
    "  color: %6;"
    "}"
    "QMenu::item:selected {"
    "  background-color: %7;"
    "  color: %8;"
    "}"
    "QMenu::separator {"
    "  height: 1px;"
    "  background: %9;"
    "  margin: %10 0;"
    "}"
    "QMenu::icon {"  // Ensure icons are visible
    "  padding-left: %4;"
    "}")
    .arg(Colors::BACKGROUND_SECONDARY)       // Menu background
    .arg(Colors::BORDER_DEFAULT)             // Border color
    .arg(Spacing::SMALL)                     // Menu padding
    .arg(Spacing::SMALL)                     // Item vertical padding
    .arg(Spacing::LARGE)                     // Item horizontal padding
    .arg(Colors::TEXT_PRIMARY)               // Text color
    .arg(Colors::ACCENT_PRIMARY)             // Selected background
    .arg(Colors::BACKGROUND_PRIMARY)         // Selected text color
    .arg(Colors::BORDER_DEFAULT)             // Separator color
    .arg(Spacing::SMALL)                     // Separator margin
    .arg("28px");                            // Left padding for icon space
}