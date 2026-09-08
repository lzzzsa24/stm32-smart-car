#include "ir_remote_keymap.h"

#include "ir_remote.h"

/* Yahboom white 21-key controller, user code 00FF.  The manufacturer's
 * printed codes are bit-reversed on the wire; these are the command bytes
 * reconstructed by this project's LSB-first NEC decoder. */
#define IR_COMMAND_AUDIO_VOLUME_UP    0x01U
#define IR_COMMAND_AUDIO_LEFT_UNUSED  0x04U
#define IR_COMMAND_CENTER_AUDIO       0x05U
#define IR_COMMAND_AUDIO_NEXT         0x06U
#define IR_COMMAND_AUDIO_VOLUME_DOWN  0x09U
#define IR_COMMAND_NUMBER_0           0x0DU
#define IR_COMMAND_NUMBER_1           0x10U
#define IR_COMMAND_NUMBER_2           0x11U
#define IR_COMMAND_NUMBER_3           0x12U
#define IR_COMMAND_NUMBER_4           0x14U
#define IR_COMMAND_NUMBER_5           0x15U

uint8_t IrRemoteKeyMap_Map(uint8_t command)
{
  switch (command)
  {
    case IR_COMMAND_NUMBER_1:
      return IR_REMOTE_VIRTUAL_KEY1;
    case IR_COMMAND_NUMBER_2:
      return IR_REMOTE_VIRTUAL_KEY2;
    case IR_COMMAND_NUMBER_3:
      return IR_REMOTE_VIRTUAL_KEY3;
    case IR_COMMAND_NUMBER_4:
      return IR_REMOTE_VIRTUAL_KEY4;
    case IR_COMMAND_NUMBER_5:
      return IR_REMOTE_VIRTUAL_KEY5;
    case IR_COMMAND_NUMBER_0:
      return IR_REMOTE_VIRTUAL_STOP;
    case IR_COMMAND_CENTER_AUDIO:
      return IR_REMOTE_VIRTUAL_AUDIO_PLAY;
    case IR_COMMAND_AUDIO_VOLUME_UP:
      return IR_REMOTE_VIRTUAL_AUDIO_VOLUME_UP;
    case IR_COMMAND_AUDIO_VOLUME_DOWN:
      return IR_REMOTE_VIRTUAL_AUDIO_VOLUME_DOWN;
    case IR_COMMAND_AUDIO_NEXT:
      return IR_REMOTE_VIRTUAL_AUDIO_NEXT;
    case IR_COMMAND_AUDIO_LEFT_UNUSED:
    default:
      return IR_REMOTE_VIRTUAL_KEY_NONE;
  }
}
