#ifndef DFPLAYER_PROTOCOL_H
#define DFPLAYER_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define DFPLAYER_PROTOCOL_PACKET_SIZE 10U

/* Build one standard 10-byte DFPlayer/YX5200 command packet.
 * feedback=0 keeps the controller independent of the module's reply wiring. */
void DfPlayerProtocol_Build(uint8_t command,
                            uint16_t parameter,
                            uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* DFPLAYER_PROTOCOL_H */
