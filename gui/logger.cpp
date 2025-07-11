#include "logger.h"
#include <QDebug>

Logger& Logger::instance()
{
  static Logger instance;
  return instance;
}

void Logger::log(Level level, const QString &message)
{
  QString prefix;
  bool isError = false;
  switch (level)
  {
  case Debug:
    prefix = "[DEBUG]";
    break;
  case Info:
    prefix = "[INFO]";
    break;
  case Warning:
    prefix = "[WARN]";
    isError = true;
    break;
  case Error:
    prefix = "[ERROR]";
    isError = true;
    break;
  }
  
  QString fullMessage = prefix + " " + message;
  qDebug() << fullMessage;
  
  // Emit signal for GUI log
  emit instance().logMessage(fullMessage, isError);
}