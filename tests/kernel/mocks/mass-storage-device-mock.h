#ifndef MASS_STORAGE_DEVICE_MOCK_H_INCLUDED
#define MASS_STORAGE_DEVICE_MOCK_H_INCLUDED

#include "stdint.h"
#include "kernel/mass-storage.h"

MassStorageDevice  massStorageDeviceMock_init(void *data, uint32_t dataSize, uint32_t blockSize);
void massStorageDeviceMock_free(MassStorageDevice device);

#endif
