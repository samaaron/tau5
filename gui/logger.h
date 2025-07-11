#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>

class Logger : public QObject
{
  Q_OBJECT
  
public:
  enum Level
  {
    Debug,
    Info,
    Warning,
    Error
  };

  static Logger& instance();
  static void log(Level level, const QString &message);

signals:
  void logMessage(const QString &message, bool isError);

private:
  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
};

#endif // LOGGER_H