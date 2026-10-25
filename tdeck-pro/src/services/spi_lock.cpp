#include "spi_lock.h"
namespace device {
SemaphoreHandle_t spiMutex=nullptr;
void spiLockBegin(){if(!spiMutex)spiMutex=xSemaphoreCreateMutex();}
}
