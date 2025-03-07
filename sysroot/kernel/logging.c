#include "kernel/logging.h"
#include "kernel/memory.h"
#include "stdarg.h"
#include "string.h"

#define ASSERTS_ENABLED
#include "utils/assert.h"

#define MAX_WRITERS_COUNT 5
#define LOG_BUFFER_SIZE 1024

typedef struct{
   LoggWriter writers[MAX_WRITERS_COUNT];
   uint32_t writerCount;
}LoggConfig;

static char *getLoggHeader(LoggLevel loggLevel, LoggContext context);

static LoggConfig config;

static LoggContext globalContext;

void logging_init(){
   config.writerCount = 0;

   globalContext = (LoggContext){
      .name = 0,
      .nestedContext = 0,
      .values = 0
   };
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

static LoggContextValue *append(LoggContextValue *list, LoggContextValue *newValue){
   newValue->next = list;
   return newValue;
}
void logging_addValueToContext(LoggContext *localContext, char *key, char *value){
   LoggContextValue *newValue = kmalloc(sizeof(LoggContextValue));
   char *keyCopy = kmalloc(strlen(key) + 1);
   char *valueCopy = kmalloc(strlen(value) + 1);
   strcpy(keyCopy, key);
   strcpy(valueCopy, value);
   *newValue = (LoggContextValue){ .key = keyCopy, .value = valueCopy };

   if(localContext->depth == 0){
      localContext->values = append(localContext->values, newValue);
   }

   LoggContext *root = &globalContext;
   while(root->nestedContext){
      root = root->nestedContext;
   }
   root->values = append(root->values, newValue);
}
static void removeValuesFromContext(LoggContext *loggContext){
   LoggContextValue *value = loggContext->values;
   while(value){
      LoggContextValue *temp = value;
      value = temp->next;

      kfree(temp->key);
      kfree(temp->value);
      kfree(temp);
   }
   loggContext->values = 0;
}

static void appendLoggContext(LoggContext *root, LoggContext *new){
   while(root->nestedContext){
      root = root->nestedContext;
   }
   root->nestedContext = new;
}
LoggContext *removeLastLoggContext(LoggContext *rootContext){
   LoggContext *prev = 0;
   while(rootContext->nestedContext){
      prev = rootContext;
      rootContext = rootContext->nestedContext;
   }

   if(prev){
      prev->nestedContext = 0;
   }
   return rootContext;
}
void logging_startLoggContext(char *name, LoggContext *localContext){
   localContext->depth++;

   char *nameCopy = kmalloc(strlen(name) + 1);
   strcpy(nameCopy, name);

   LoggContext *newContext = kmalloc(sizeof(LoggContext));
   *newContext = (LoggContext){
      .name = nameCopy,
      .values = 0,
      .nestedContext = 0
   };
   appendLoggContext(&globalContext, newContext);
}

void logging_endLoggContext(LoggContext *localContext){
   localContext->depth--;
   LoggContext *lastContext = removeLastLoggContext(&globalContext);

   kfree(lastContext->name);
   removeValuesFromContext(lastContext);
   kfree(lastContext);
}

void logging_log(LoggContext context, LoggLevel loggLevel, char *data, ...){
   va_list args;
   va_start(args, data);
   logging_vlog(context, loggLevel, data, args);
   va_end(args);
}

void logging_vlog(LoggContext context, LoggLevel loggLevel, char *data, va_list args){
   char *header = getLoggHeader(loggLevel, context);

   char *message = kmalloc(LOG_BUFFER_SIZE + 1);
   int length = vsnprintf(message, LOG_BUFFER_SIZE, data, args);
   if(length > LOG_BUFFER_SIZE){
      kfree(message);
      message = kmalloc(length + 1);
      vsnprintf(message, length, data, args);
   }

   char *log = kmalloc(strlen(header) + strlen(message) + 1);

   strcpy(log, header);
   strAppend(log, message);
   kfree(message);
   kfree(header);

   for(uint32_t i = 0; i < config.writerCount; i++){  
      if(loggLevel >= config.writers[i].loggLevel){
         if(config.writers[i].writerType == DefaultWriter){
            config.writers[i].write(log);
         }else if(config.writers[i].writerType == CustomWriter){
            config.writers[i].customwrite(context, loggLevel, data, args);
         }
      }
   }

   kfree(log);
}

static int getContextValueStrLength(const LoggContext *context){
   int length = 0;
   LoggContextValue *list = context->values;
   while(list){
      length += 6 + strlen(list->key) + strlen(list->value);
      list = list->next;
   }
   return length;
}

static char *formatSingleContext(const LoggContext *context){
   int nameLength = strlen(context->name);
   char *currContext = kmalloc(nameLength + 7 + getContextValueStrLength(context));

   char *ptr = currContext;
   ptr += sprintf(ptr, "[%s", context->name);

   LoggContextValue *list = context->values;
   if(list){
      ptr = strAppend(ptr, " ");
      while(list){
         ptr += sprintf(ptr, "[%s: %s] ", list->key, list->value);
         list = list->next;
      }
      ptr--;
      *ptr = 0;
   }
   ptr = strAppend(ptr, "] ");
   return currContext;
}

static char *formatContext(const LoggContext *globalContext, const LoggContext *localContext){
   if(!globalContext){
      return formatSingleContext(localContext);
   }

   char *nestedContext = formatContext(globalContext->nestedContext, localContext);
   char *currContext = formatSingleContext(globalContext);

   char *result = kmalloc(strlen(nestedContext) + strlen(currContext) + 5);
   strcpy(result, currContext);
   strAppend(result, nestedContext);
   kfree(nestedContext);
   kfree(currContext);

   return result;
}

static char* getLoggHeader(LoggLevel loggLevel, LoggContext localContext){
   char *contextStr = formatContext(globalContext.nestedContext, &localContext);
   char *buffer = kmalloc(15 + strlen(contextStr));

   char *ptr = buffer;
   ptr = strcpy(ptr, contextStr);
   kfree(contextStr);

   switch(loggLevel){
      case LoggLevelDebug:
         sprintf(ptr, "Debug: ", localContext.name);
         break;
      case LoggLevelInfo:
         sprintf(ptr, "Info: ", localContext.name);
         break;
      case LoggLevelWarning:
         sprintf(ptr, "Warning: ", localContext.name);
         break;
      case LoggLevelError:
         sprintf(ptr, "Error: ", localContext.name);
         break;
      default:
         strcpy(ptr, "Unknown: ");
         break;
   }  
   return buffer;
}
