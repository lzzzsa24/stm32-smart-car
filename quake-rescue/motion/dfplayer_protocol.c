#include "dfplayer_protocol.h"

void DfPlayerProtocol_Build(uint8_t command,
                            uint16_t parameter,
                            uint8_t packet[DFPLAYER_PROTOCOL_PACKET_SIZE])
{
  uint8_t parameter_high;
  uint8_t parameter_low;
  uint16_t checksum;

  if (packet == 0)
  {
    return;
  }

  parameter_high = (uint8_t)(parameter >> 8U);
  parameter_low = (uint8_t)(parameter & 0xFFU);
  checksum = (uint16_t)(0U - (uint16_t)(0xFFU + 0x06U + command +
                                        parameter_high + parameter_low));

  packet[0] = 0x7EU;
  packet[1] = 0xFFU;
  packet[2] = 0x06U;
  packet[3] = command;
  packet[4] = 0x00U;
  packet[5] = parameter_high;
  packet[6] = parameter_low;
  packet[7] = (uint8_t)(checksum >> 8U);
  packet[8] = (uint8_t)(checksum & 0xFFU);
  packet[9] = 0xEFU;
}
