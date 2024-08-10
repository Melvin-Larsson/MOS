#ifndef LOGGING_H_INCLUDED
#define LOGGING_H_INCLUDED

#include "stdint.h"
#include "stdarg.h"

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

typedef enum{
   LoggingOk,
   LoggingMaximumWritersConfigured,
}LoggStatus;

typedef enum{
   LoggLevelDebug = 0,
   LoggLevelInfo = 1,
   LoggLevelWarning = 2,
   LoggLevelError = 3,
   LoggLevelNone = 1000
}LoggLevel;

typedef enum{
   DefaultWriter,
   CustomWriter
}WriterType;

typedef struct{
   char *name;
}LoggContext;

typedef struct{
   union{
      void (*write)(const char *data);
      void (*writef)(const char *data, ...);
      void (*customwrite)(LoggContext, LoggLevel, const char *data, va_list args);
   };
   LoggLevel loggLevel;
   WriterType writerType;
}LoggWriter;

void logging_init();
void logging_log(LoggContext context, LoggLevel loggLevel, char *data, ...);
void logging_vlog(LoggContext context, LoggLevel loggLevel, char *data, va_list args);
LoggWriter logging_getDefaultWriter(void (*write)(const char *data));
LoggWriter logging_getDefaultFormatWriter(void (*writef)(const char *data, ...));
LoggWriter logging_getCustomWriter(
      void (*write)(LoggContext,LoggLevel, const char *data, va_list));
LoggStatus logging_addWriter(LoggWriter writer);

LoggContext updateLoggContext(LoggContext loggContext, char *name);

static LoggContext loggContext = {
   .name = 0
};

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define loggDebug(...) logging_log(updateLoggContext(loggContext, __FILE__), LoggLevelDebug,  __VA_ARGS__)
#else
#define loggDebug(...) {}
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define loggInfo(...) logging_log(updateLoggContext(loggContext, __FILE__), LoggLevelInfo,  __VA_ARGS__)
#else
#define loggInfo(...) {}
#endif


#if LOG_LEVEL <= LOG_LEVEL_WARNING
#define loggWarning(...) logging_log(updateLoggContext(loggContext, __FILE__), LoggLevelWarning,  __VA_ARGS__)
#else
#define loggWarning(...) {}
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define loggError(...) logging_log(updateLoggContext(loggContext, __FILE__), LoggLevelError,  __VA_ARGS__)
#else
#define loggError(...) {}
#endif

#endif
