#include "kernel/logging.h"

#include "stdarg.h"
#include "string.h"
#include "stdlib.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

#define MAX_WRITERS_COUNT 5

typedef struct{
   LoggWriter writers[MAX_WRITERS_COUNT];
   uint32_t writerCount;
}LoggConfig;

static char *getLoggHeader(LoggLevel loggLevel, LoggContext context);

static LoggConfig config;

void logging_init(){
   config.writerCount = 0;
}

LoggWriter logging_getDefaultFormatWriter(void (*writef)(const char *data, ...)){
   return logging_getDefaultWriter((void*)(const char *)writef);
}
LoggWriter logging_getDefaultWriter(void (*write) (const char *data)){
   return (LoggWriter){
      .write = write,
      .loggLevel = LoggLevelInfo,
      .writerType = DefaultWriter
   };
}
LoggWriter logging_getCustomWriter(void (*customwriter)(LoggContext,LoggLevel, const char *data, va_list)){
   return (LoggWriter){
      .customwrite = customwriter,
      .loggLevel = LoggLevelInfo,
      .writerType = CustomWriter
   };
}

LoggStatus logging_addWriter(LoggWriter writer){
   if(config.writerCount >= MAX_WRITERS_COUNT) {
      return LoggingMaximumWritersConfigured;
   }

   config.writers[config.writerCount] = writer;
   config.writerCount++;

   return LoggingOk;
}

LoggContext updateLoggContext(LoggContext loggContext, char *name){
   if(loggContext.name){
      return loggContext;
   }
   loggContext.name = name;
   return loggContext;
}

void logging_log(LoggContext context, LoggLevel loggLevel, char *data, ...){
   va_list args;
   va_start(args, data);
   logging_vlog(context, loggLevel, data, args);
   va_end(args);
}

void logging_vlog(LoggContext context, LoggLevel loggLevel, char *data, va_list args){
   char *header = getLoggHeader(loggLevel, context);

//    int length = strlen(data);
   char *message = malloc(4096); //FIXME: Unsafe

   vsprintf(message, data, args);

   char *log = malloc(4096);
   static int count = 0;
   count++;


   strcpy(log, header);
   strAppend(log, message);
   free(message);
   free(header);


   for(uint32_t i = 0; i < config.writerCount; i++){  
      if(loggLevel >= config.writers[i].loggLevel){
         if(config.writers[i].writerType == DefaultWriter){
            config.writers[i].write(log);
         }else if(config.writers[i].writerType == CustomWriter){
            config.writers[i].customwrite(context, loggLevel, data, args);
         }
      }
   }

   free(log);
}

static char* getLoggHeader(LoggLevel loggLevel, LoggContext context){
   int nameLength = strlen(context.name);
   char *buffer = malloc(nameLength + 15);
   switch(loggLevel){
      case LoggLevelDebug:
         sprintf(buffer, "[%s] Debug: ", context.name);
         break;
      case LoggLevelInfo:
         sprintf(buffer, "[%s] Info: ", context.name);
         break;
      case LoggLevelWarning:
         sprintf(buffer, "[%s] Warning: ", context.name);
         break;
      case LoggLevelError:
         sprintf(buffer, "[%s] Error: ", context.name);
         break;
      default:
         strcpy(buffer, "[%s] Unknown: ");
         break;
   }  
   return buffer;
}
