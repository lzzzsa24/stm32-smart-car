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
  static const uint8_t previous[] =
      {0x7E, 0xFF, 0x06, 0x02, 0x00, 0x00, 0x00, 0xFE, 0xF9, 0xEF};
  static const uint8_t play_physical_7[] =
      {0x7E, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x07, 0xFE, 0xF1, 0xEF};
  static const uint8_t start[] =
      {0x7E, 0xFF, 0x06, 0x0D, 0x00, 0x00, 0x00, 0xFE, 0xEE, 0xEF};
  static const uint8_t pause[] =
      {0x7E, 0xFF, 0x06, 0x0E, 0x00, 0x00, 0x00, 0xFE, 0xED, 0xEF};
  static const uint8_t query_sd_track[] =
      {0x7E, 0xFF, 0x06, 0x4C, 0x00, 0x00, 0x00, 0xFE, 0xAF, 0xEF};
  static const uint8_t loop_current[] =
      {0x7E, 0xFF, 0x06, 0x19, 0x00, 0x00, 0x00, 0xFE, 0xE2, 0xEF};

  expect_packet(0x12U, 1U, play_mp3_0001);
  expect_packet(0x06U, 10U, volume_10);
  expect_packet(0x16U, 0U, stop);
  expect_packet(0x01U, 0U, next);
  expect_packet(0x02U, 0U, previous);
  expect_packet(0x03U, 7U, play_physical_7);
  expect_packet(0x0DU, 0U, start);
  expect_packet(0x0EU, 0U, pause);
  expect_packet(0x4CU, 0U, query_sd_track);
  expect_packet(0x19U, 0U, loop_current);
  DfPlayerProtocol_Build(0x12U, 1U, 0);

  /* Manufacturer's printed 80/A0/60/90 direction codes are bit-reversed by
     this NEC decoder into 01/05/06/09. */
  assert(IrRemoteKeyMap_Map(0x01U) == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_UP);
  assert(IrRemoteKeyMap_Map(0x05U) == IR_REMOTE_VIRTUAL_AUDIO_TOGGLE);
  assert(IrRemoteKeyMap_Map(0x06U) == IR_REMOTE_VIRTUAL_AUDIO_NEXT);
  assert(IrRemoteKeyMap_Map(0x09U) == IR_REMOTE_VIRTUAL_AUDIO_VOLUME_DOWN);
  assert(IrRemoteKeyMap_Map(0x04U) == IR_REMOTE_VIRTUAL_AUDIO_PREVIOUS);
  assert(IrRemoteKeyMap_Map(0x10U) == IR_REMOTE_VIRTUAL_KEY1);
  assert(IrRemoteKeyMap_Map(0x11U) == IR_REMOTE_VIRTUAL_KEY2);
  assert(IrRemoteKeyMap_Map(0x12U) == IR_REMOTE_VIRTUAL_KEY3);
  assert(IrRemoteKeyMap_Map(0x14U) == IR_REMOTE_VIRTUAL_KEY4);
  assert(IrRemoteKeyMap_Map(0x15U) == IR_REMOTE_VIRTUAL_KEY5);
  assert(IrRemoteKeyMap_Map(0x0DU) == IR_REMOTE_VIRTUAL_STOP);

  puts("dfplayer protocol and remote keymap tests passed");
  return 0;
}
