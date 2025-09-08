#include "qt_message_handler.h"
#include "tau5logger.h"
#include <QString>

namespace Tau5Common {

static QtMessageHandler originalMessageHandler = nullptr;

static void unifiedMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Call original handler if it exists
    if (originalMessageHandler) {
        originalMessageHandler(type, context, msg);
    }

    // Route to Tau5Logger with appropriate level
    switch (type) {
    case QtDebugMsg:
        Tau5Logger::instance().debug(QString("[Qt] %1").arg(msg));
        break;
    case QtInfoMsg:
        Tau5Logger::instance().info(QString("[Qt] %1").arg(msg));
        break;
    case QtWarningMsg:
        Tau5Logger::instance().warning(QString("[Qt] %1").arg(msg));
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        Tau5Logger::instance().error(QString("[Qt] %1").arg(msg));
        break;
    }
}

void installQtMessageHandler()
{
    originalMessageHandler = qInstallMessageHandler(unifiedMessageHandler);
}

} // namespace Tau5Common