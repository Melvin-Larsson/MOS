#include "kernel/logging.h"

#define UNUSED(x) (void)(x)

void logging_init(){}
void logging_log(LoggContext context, LoggLevel loggLevel, char *data, ...){
      UNUSED(context);
      UNUSED(loggLevel);
      UNUSED(data);
}
void logging_vlog(LoggContext context, LoggLevel loggLevel, char *data, va_list args){
      UNUSED(args);
      UNUSED(context);
      UNUSED(loggLevel);
      UNUSED(data);
}
LoggWriter logging_getDefaultWriter(void (*write)(const char *data)){
      UNUSED(write);
      return (LoggWriter){};
}
LoggWriter logging_getDefaultFormatWriter(void (*writef)(const char *data, ...)){
      UNUSED(writef);
      return (LoggWriter){};
}
LoggWriter logging_getCustomWriter(
            void (*write)(LoggContext,LoggLevel, const char *data, va_list)){

      UNUSED(write);
      return (LoggWriter){};
}
LoggStatus logging_addWriter(LoggWriter writer){
      UNUSED(writer);
      return LoggingOk;
}

LoggContext updateLoggContext(LoggContext loggContext, char *name){
      UNUSED(name);
      return loggContext;
}
void logging_addValueToContext(LoggContext *loggContext, char *key, char *value){
      UNUSED(loggContext);
      UNUSED(key);
      UNUSED(value);
}
void logging_startLoggContext(char *name, LoggContext *localContext){
      UNUSED(name);
      UNUSED(localContext);
}
void logging_endLoggContext(LoggContext *localContext){
      UNUSED(localContext);
}
