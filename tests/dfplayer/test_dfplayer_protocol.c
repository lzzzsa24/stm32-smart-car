#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dfplayer_protocol.h"
#include "ir_remote.h"
#include "ir_remote_keymap.h"

static void expect_packet(uint8_t command,
                          uint16_t parameter,
                          const uint8_t expected[DFPLAYER_PROTOCOL_PACKET_SIZE])
{
  uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE];
  DfPlayerProtocol_Build(command, parameter, packet);
  assert(memcmp(packet, expected, sizeof(packet)) == 0);
}

int main(void)
{
  static const uint8_t play_mp3_0001[] =
      {0x7E, 0xFF, 0x06, 0x12, 0x00, 0x00, 0x01, 0xFE, 0xE8, 0xEF};
  static const uint8_t volume_10[] =
      {0x7E, 0xFF, 0x06, 0x06, 0x00, 0x00, 0x0A, 0xFE, 0xEB, 0xEF};
  static const uint8_t stop[] =
      {0x7E, 0xFF, 0x06, 0x16, 0x00, 0x00, 0x00, 0xFE, 0xE5, 0xEF};
  static const uint8_t next[] =
      {0x7E, 0xFF, 0x06, 0x01, 0x00, 0x00, 0x00, 0xFE, 0xFA, 0xEF};
  static const uint8_t loop_current[] =
      {0x7E, 0xFF, 0x06, 0x19, 0x00, 0x00, 0x00, 0xFE, 0xE2, 0xEF};

  expect_packet(0x12U, 1U, play_mp3_0001);
  expect_packet(0x06U, 10U, volume_10);
  expect_packet(0x16U, 0U, stop);
  expect_packet(0x01U, 0U, next);
  expect_packet(0x19U, 0U, loop_current);
  DfPlayerProtocol_Build(0x12U, 1U, 0);

  /* Manufacturer's printed 80/A0/60/90 direction codes are bit-reversed by
     this NEC decoder into 01/05/06/09. */
  assert(IrRemoteKeyMap_Map(0x01U) == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_UP);
  assert(IrRemoteKeyMap_Map(0x05U) == IR_REMOTE_VIRTUAL_AUDIO_PLAY);
  assert(IrRemoteKeyMap_Map(0x06U) == IR_REMOTE_VIRTUAL_AUDIO_NEXT);
  assert(IrRemoteKeyMap_Map(0x09U) == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_DOWN);
  assert(IrRemoteKeyMap_Map(0x04U) == IR_REMOTE_VIRTUAL_KEY_NONE);
  assert(IrRemoteKeyMap_Map(0x10U) == IR_REMOTE_VIRTUAL_KEY1);
  assert(IrRemoteKeyMap_Map(0x11U) == IR_REMOTE_VIRTUAL_KEY2);
  assert(IrRemoteKeyMap_Map(0x12U) == IR_REMOTE_VIRTUAL_KEY3);
  assert(IrRemoteKeyMap_Map(0x14U) == IR_REMOTE_VIRTUAL_KEY4);
  assert(IrRemoteKeyMap_Map(0x15U) == IR_REMOTE_VIRTUAL_KEY5);
  assert(IrRemoteKeyMap_Map(0x0DU) == IR_REMOTE_VIRTUAL_STOP);

  puts("dfplayer protocol and remote keymap tests passed");
  return 0;
}
