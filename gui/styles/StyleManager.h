#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include <QString>
#include <QColor>

class StyleManager
{
public:
  // Color Palette with semantic naming
  struct Colors
  {
    // Primary brand colors
    static const QString ACCENT_PRIMARY;          // Primary accent color (orange)
    static const QString ACCENT_PRIMARY_RGB;      // Primary accent in RGB format
    static const QString ACCENT_SECONDARY;        // Secondary accent (deep pink)
    static const QString ACCENT_HIGHLIGHT;        // Highlight color (blue)
    
    // Background colors
    static const QString BACKGROUND_PRIMARY;      // Main background (black)
    static const QString BACKGROUND_SECONDARY;    // Secondary background (dark gray)
    static const QString BACKGROUND_TERTIARY;     // Tertiary background (medium dark)
    static const QString BACKGROUND_CONSOLE;      // Console specific background
    static const QString BACKGROUND_SURFACE;      // Surface/card background
    static const QString BACKGROUND_SURFACE_LIGHT;// Lighter surface background
    static const QString BACKGROUND_TITLEBAR_DARK;// macOS dark mode titlebar color
    
    // Text colors
    static const QString TEXT_PRIMARY;            // Primary text (white)
    static const QString TEXT_SECONDARY;          // Secondary text (light gray)
    static const QString TEXT_TERTIARY;           // Tertiary text (medium gray)
    static const QString TEXT_MUTED;              // Muted text (dark gray)
    static const QString TEXT_TIMESTAMP;          // Timestamp specific text
    
    // Interactive element colors
    static const QString SCROLLBAR_TRACK;         // Scrollbar track color
    static const QString SCROLLBAR_THUMB;         // Scrollbar thumb color
    static const QString SCROLLBAR_THUMB_HOVER;   // Scrollbar thumb hover
    static const QString BUTTON_HOVER;            // Button hover state
    static const QString BUTTON_ACTIVE;           // Button active/pressed state
    static const QString BUTTON_CLOSE_HOVER;      // Close button hover (red)
    
    // Special purpose colors
    static const QString TERMINAL_CURSOR;         // Terminal cursor color
    static const QString SELECTION_BACKGROUND;    // Text selection background
    static const QString SELECTION_TEXT;          // Text selection foreground
    static const QString BORDER_DEFAULT;          // Default border color
    static const QString SCANLINE_OVERLAY;        // CRT scanline effect
    
    // Status colors
    static const QString STATUS_ERROR;            // Error status (currently blue)
    static const QString STATUS_WARNING;          // Warning status
    static const QString STATUS_SUCCESS;          // Success status
    static const QString STATUS_INFO;             // Info status

    // Alpha variants for transparency
    static QString accentPrimaryAlpha(int alpha);
    static QString backgroundPrimaryAlpha(int alpha);
    static QString textPrimaryAlpha(int alpha);
    static QString statusErrorAlpha(int alpha);
    
    // Alpha conversion helpers (0.0-1.0)
    static QString accentPrimaryAlpha(double alpha);
    static QString backgroundPrimaryAlpha(double alpha);
    static QString statusErrorAlpha(double alpha);
    
    // Legacy color name mappings (for backward compatibility)
    static const QString& PRIMARY_ORANGE;
    static const QString& PRIMARY_ORANGE_RGB;
    static const QString& ERROR_BLUE;
    static const QString& TIMESTAMP_GRAY;
    static const QString& BLACK;
    static const QString& WHITE;
    static const QString& DEEP_PINK;
    static const QString& DARK_BACKGROUND;
    static const QString& CONSOLE_BACKGROUND;
    
    // Legacy alpha functions
    static QString primaryOrangeAlpha(int alpha) { return accentPrimaryAlpha(alpha); }
    static QString primaryOrangeAlpha(double alpha) { return accentPrimaryAlpha(alpha); }
    static QString blackAlpha(int alpha) { return backgroundPrimaryAlpha(alpha); }
    static QString blackAlpha(double alpha) { return backgroundPrimaryAlpha(alpha); }
    static QString whiteAlpha(int alpha) { return textPrimaryAlpha(alpha); }
    static QString errorBlueAlpha(int alpha) { return statusErrorAlpha(alpha); }
    static QString errorBlueAlpha(double alpha) { return statusErrorAlpha(alpha); }
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
  static QString contextMenu();
  static QString tooltip();

private:
  StyleManager() = default; // Static class, no instantiation
};

#endif // STYLEMANAGER_H