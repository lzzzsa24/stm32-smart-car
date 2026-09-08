#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dfplayer_protocol.h"

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

  expect_packet(0x12U, 1U, play_mp3_0001);
  expect_packet(0x06U, 10U, volume_10);
  expect_packet(0x16U, 0U, stop);
  DfPlayerProtocol_Build(0x12U, 1U, 0);

  puts("dfplayer protocol tests passed");
  return 0;
}
