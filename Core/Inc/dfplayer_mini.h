#ifndef DFPLAYER_MINI_H
#define DFPLAYER_MINI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * YB-DSF01-V1.1 J8 wiring:
 *   J8-1 5V       -> DFPlayer VCC
 *   J8-2 PC11/RX  <- DFPlayer TX
 *   J8-3 PC10/TX  -> DFPlayer RX (680 ohm to 1 kohm series recommended)
 *   J8-4 GND      -> DFPlayer GND
 * Speaker: DFPlayer SPK1/SPK2 directly to one 8-ohm speaker.
 *
 * The driver uses UART4 at 9600 8N1.  It waits for the player/card to boot,
 * transmits from UART4 IRQ, and never busy-waits in the application loop.
 */

#ifndef DFPLAYER_MINI_BOOT_DELAY_MS
#define DFPLAYER_MINI_BOOT_DELAY_MS       3000U
#endif

#ifndef DFPLAYER_MINI_COMMAND_GAP_MS
#define DFPLAYER_MINI_COMMAND_GAP_MS        80U
#endif

#ifndef DFPLAYER_MINI_DEFAULT_VOLUME
#define DFPLAYER_MINI_DEFAULT_VOLUME        10U
#endif

void DfPlayerMini_Init(void);
void DfPlayerMini_Task(uint32_t now_ms);

/* Plays /mp3/NNNN.mp3; valid track numbers are 1..65535.
 * A request made during the boot delay is retained and sent when ready. */
uint8_t DfPlayerMini_PlayMp3Track(uint16_t track_number);

/* Volume range is 0..30.  The latest request replaces an older pending one. */
uint8_t DfPlayerMini_SetVolume(uint8_t volume);

/* Cancels a pending play and sends one STOP command if playback was requested. */
void DfPlayerMini_Stop(void);

uint8_t DfPlayerMini_IsReady(void);

/* Call only from the project's UART4_IRQHandler(). */
void DfPlayerMini_UART4_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* DFPLAYER_MINI_H */
