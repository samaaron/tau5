#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QString>
#include <QColor>

class StyleManager
{
public:
  // Color Palette
  struct Colors
  {
    static const QString PRIMARY_ORANGE;
    static const QString PRIMARY_ORANGE_RGB;
    static const QString ERROR_BLUE;
    static const QString TIMESTAMP_GRAY;
    static const QString BLACK;
    static const QString WHITE;
    static const QString DEEP_PINK;
    static const QString DARK_BACKGROUND;
    static const QString CONSOLE_BACKGROUND;

    // Alpha variants for transparency
    static QString primaryOrangeAlpha(int alpha);
    static QString blackAlpha(int alpha);
    static QString whiteAlpha(int alpha);
    static QString errorBlueAlpha(int alpha);
    
    // Alpha conversion helpers
    static QString primaryOrangeAlpha(double alpha);  // Takes 0.0-1.0
    static QString blackAlpha(double alpha);
    static QString errorBlueAlpha(double alpha);
  };

  // Typography
  struct Typography
  {
    static const QString MONOSPACE_FONT_FAMILY;
    static const QString DEFAULT_FONT_FAMILY;

    static const QString FONT_SIZE_SMALL;  // 10px
    static const QString FONT_SIZE_MEDIUM; // 12px
    static const QString FONT_SIZE_LARGE;  // 14px

    static const QString FONT_WEIGHT_NORMAL;
    static const QString FONT_WEIGHT_BOLD;
  };

  // Spacing
  struct Spacing
  {
    static const QString EXTRA_SMALL; // 2px
    static const QString SMALL;       // 4px
    static const QString MEDIUM;      // 8px
    static const QString LARGE;       // 12px
    static const QString EXTRA_LARGE; // 16px
  };

  // Common Style Components
  static QString darkGradientBackground();
  static QString headerGradientBackground();
  static QString primaryButton();
  static QString secondaryButton();
  static QString tau5Scrollbar();
  static QString orangeBorder(const QString &width = "1px");
  static QString textEdit();
  static QString checkbox();

  // Component-specific styles
  static QString consoleHeader();
  static QString consoleOutput();
  static QString consoleScrollbar();
  static QString guiButton();
  static QString mainWindow();

private:
  StyleManager() = default; // Static class, no instantiation
};

#endif // STYLEMANAGER_H