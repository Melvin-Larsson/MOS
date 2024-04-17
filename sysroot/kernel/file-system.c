#include "kernel/file-system.h"
#include "stdlib.h"

void directoryEntry_free(DirectoryEntry *entry){
   free(entry->path);
   free(entry->filename);
   free(entry);
}

