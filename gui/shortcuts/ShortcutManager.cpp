#include "ShortcutManager.h"
#include "../shared/tau5logger.h"
#include <QShortcut>
#include <QAction>
#include <QWidget>
#include <QDebug>

ShortcutManager& ShortcutManager::instance()
{
    static ShortcutManager instance;
    return instance;
}

ShortcutManager::ShortcutManager()
{
    initializeDefaultShortcuts();
}

// Platform-specific helper functions (Sonic Pi style)
// Cmd on Mac, Ctrl everywhere else
QKeySequence ShortcutManager::ctrlKey(const QString& key)
{
#ifdef Q_OS_MAC
    return QKeySequence(QString("Meta+%1").arg(key));
#else
    return QKeySequence(QString("Ctrl+%1").arg(key));
#endif
}

// Ctrl on Mac, Alt everywhere else
QKeySequence ShortcutManager::metaKey(const QString& key)
{
#ifdef Q_OS_MAC
    return QKeySequence(QString("Ctrl+%1").arg(key));
#else
    return QKeySequence(QString("Alt+%1").arg(key));
#endif
}

QKeySequence ShortcutManager::ctrlShiftKey(const QString& key)
{
#ifdef Q_OS_MAC
    return QKeySequence(QString("Shift+Meta+%1").arg(key));
#else
    return QKeySequence(QString("Shift+Ctrl+%1").arg(key));
#endif
}

QKeySequence ShortcutManager::shiftMetaKey(const QString& key)
{
#ifdef Q_OS_MAC
    return QKeySequence(QString("Shift+Ctrl+%1").arg(key));
#else
    return QKeySequence(QString("Shift+Alt+%1").arg(key));
#endif
}

void ShortcutManager::initializeDefaultShortcuts()
{

    registerShortcut(DebugPaneSearch, ctrlKey("S"),
                    "Search/Find Next in Debug Pane", DebugPane);
    registerShortcut(DebugPaneFindNext, ctrlKey("S"),
                    "Find Next Match", DebugPane);
    registerShortcut(DebugPaneFindPrevious, ctrlKey("R"),
                    "Find Previous Match", DebugPane);
    registerShortcut(DebugPaneCloseSearch, ctrlKey("G"),
                    "Close Search", DebugPane);
}

void ShortcutManager::registerShortcut(ShortcutId id, const QKeySequence& keySequence,
                                      const QString& description, ShortcutCategory category)
{
    m_shortcuts[id] = {keySequence, description, category, true};

    Tau5Logger::instance().debug(QString("Registered shortcut: %1 Key sequence: %2 Native: %3")
                                  .arg(description)
                                  .arg(keySequence.toString())
                                  .arg(keySequence.toString(QKeySequence::NativeText)));
}


void ShortcutManager::bindToAction(ShortcutId id, QAction* action)
{
    if (!action) return;

    auto it = m_shortcuts.find(id);
    if (it == m_shortcuts.end()) {
        qWarning() << "Unknown shortcut ID:" << id;
        return;
    }

    const ShortcutInfo& info = it.value();

    action->setShortcut(info.keySequence);
    action->setToolTip(QString("%1 (%2)").arg(info.description)
                                        .arg(info.keySequence.toString(QKeySequence::NativeText)));

        QObject::connect(action, &QAction::triggered, this, [this, id]() {
        emit shortcutTriggered(id);
    });
}

QKeySequence ShortcutManager::getKeySequence(ShortcutId id) const
{
    auto it = m_shortcuts.find(id);
    return (it != m_shortcuts.end()) ? it.value().keySequence : QKeySequence();
}

QString ShortcutManager::getDescription(ShortcutId id) const
{
    auto it = m_shortcuts.find(id);
    return (it != m_shortcuts.end()) ? it.value().description : QString();
}

ShortcutManager::ShortcutCategory ShortcutManager::getCategory(ShortcutId id) const
{
    auto it = m_shortcuts.find(id);
    return (it != m_shortcuts.end()) ? it.value().category : DebugPane;
}



QList<ShortcutManager::ShortcutId> ShortcutManager::getShortcutsInCategory(ShortcutCategory category) const
{
    QList<ShortcutId> result;
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        if (it.value().category == category) {
            result.append(it.key());
        }
    }
    return result;
}