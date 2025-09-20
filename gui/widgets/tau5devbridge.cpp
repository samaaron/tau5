#include "tau5devbridge.h"
#include "../shared/tau5logger.h"

Tau5DevBridge::Tau5DevBridge(QObject *parent)
    : QObject(parent)
{
}

void Tau5DevBridge::hardRefresh()
{
    Tau5Logger::instance().info("[Tau5DevBridge] Hard refresh requested from JavaScript");
    emit hardRefreshRequested();
}