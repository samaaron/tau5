#ifndef QT_MESSAGE_HANDLER_H
#define QT_MESSAGE_HANDLER_H

#include <QtGlobal>

namespace Tau5Common {
    /**
     * Install unified Qt message handler that routes to Tau5Logger
     * This replaces the duplicated tau5MessageHandler and tau5NodeMessageHandler
     * All Qt debug/info/warning/error messages will be routed through Tau5Logger
     * with appropriate log levels and [Qt] prefix
     */
    void installQtMessageHandler();
}

#endif // QT_MESSAGE_HANDLER_H