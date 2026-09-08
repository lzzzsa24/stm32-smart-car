#ifndef IR_REMOTE_KEYMAP_H
#define IR_REMOTE_KEYMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Convert the NEC command byte reconstructed LSB-first by ir_remote.c into
 * one application virtual key. */
uint8_t IrRemoteKeyMap_Map(uint8_t command);

#ifdef __cplusplus
}
#endif

#endif /* IR_REMOTE_KEYMAP_H */
