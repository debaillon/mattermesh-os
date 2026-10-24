#pragma once
// Device identity shown to the mesh (manufacturer name), the CLI and the HTTP User-Agent of maps and weather.
#ifdef MM_BOARD_PLUS
#define MM_DEVICE_MODEL "T-Deck Plus"
#define MM_DEVICE_TOKEN "TDeckPlus"
#else
#define MM_DEVICE_MODEL "T-Deck Max"
#define MM_DEVICE_TOKEN "TDeckMax"
#endif
