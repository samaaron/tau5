#ifndef SHORTCUTMANAGER_H
#define SHORTCUTMANAGER_H

#include <QObject>
#include <QKeySequence>
#include <QMap>
#include <QList>
#include <memory>

class QShortcut;
class QAction;
class QWidget;

class ShortcutManager : public QObject
{
  Q_OBJECT

public:
  // Shortcut identifiers
  enum ShortcutId
  {
    // Debug pane shortcuts
    DebugPaneSearch,       // Ctrl+S when search visible: find next, otherwise toggle search
    DebugPaneFindNext,     // Also Ctrl+S when search is visible
    DebugPaneFindPrevious, // Ctrl+R
    DebugPaneCloseSearch   // Ctrl+G
  };

  // Shortcut categories for organization
  enum ShortcutCategory
  {
    DebugPane
  };

  static ShortcutManager &instance();

  // Register a shortcut
  void registerShortcut(ShortcutId id, const QKeySequence &keySequence,
                        const QString &description, ShortcutCategory category = DebugPane);

  // Bind shortcut to an existing QAction
  void bindToAction(ShortcutId id, QAction *action);

  // Get shortcut info
  QKeySequence getKeySequence(ShortcutId id) const;
  QString getDescription(ShortcutId id) const;
  ShortcutCategory getCategory(ShortcutId id) const;

  // Get all shortcuts in a category
  QList<ShortcutId> getShortcutsInCategory(ShortcutCategory category) const;

  // Platform-specific helper functions (Sonic Pi style)
  static QKeySequence ctrlKey(const QString &key);      // Cmd on Mac, Ctrl elsewhere
  static QKeySequence metaKey(const QString &key);      // Ctrl on Mac, Alt elsewhere
  static QKeySequence ctrlShiftKey(const QString &key); // Cmd+Shift on Mac, Ctrl+Shift elsewhere
  static QKeySequence shiftMetaKey(const QString &key); // Ctrl+Shift on Mac, Alt+Shift elsewhere

signals:
  void shortcutTriggered(ShortcutId id);

private:
  ShortcutManager();
  ~ShortcutManager() = default;
  ShortcutManager(const ShortcutManager &) = delete;
  ShortcutManager &operator=(const ShortcutManager &) = delete;

  void initializeDefaultShortcuts();

  struct ShortcutInfo
  {
    QKeySequence keySequence;
    QString description;
    ShortcutCategory category;
    bool enabled = true;
  };

  QMap<ShortcutId, ShortcutInfo> m_shortcuts;
};

#endif // SHORTCUTMANAGER_H