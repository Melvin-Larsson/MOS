#include "kernel/file-system.h"
#include "kernel/memory.h"

void directoryEntry_free(DirectoryEntry *entry){
   kfree(entry->path);
   kfree(entry->filename);
   kfree(entry);
}

