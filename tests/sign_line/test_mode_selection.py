"""Compile the real enum and input selector from main.c with input/audio stubs."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / "Core/Src/main.c").read_text(encoding="utf-8")
enum = re.search(r"typedef enum\s*\{\s*APP_MODE_INTEGRATED[^}]+\} AppMode;", main)
assert enum
start = main.index("static AppMode read_requested_mode(AppMode current_mode)\n{")
opening = main.index("{", start)
depth = 1
end = opening + 1
while depth:
    depth += (main[end] == "{") - (main[end] == "}")
    end += 1
selector = main[start:end]
assert "case '4': return IR_REMOTE_VIRTUAL_KEY4;" in main

stubs = r'''
#include <assert.h>
#include <stdio.h>
#include "ir_remote.h"
#define GPIO_PIN_RESET 0
#define key1_GPIO_Port 0
#define key2_GPIO_Port 0
#define key3_GPIO_Port 0
#define key1_Pin 1
#define key2_Pin 2
#define key3_Pin 3
#define EXP7_AUDIO_VOLUME_STEP 2
static uint8_t remote_input, serial_input, button;
static unsigned stops;
uint8_t IrRemote_TakeVirtualKey(void) { return remote_input; }
static uint8_t app_take_serial_virtual_key(void) { return serial_input; }
static int HAL_GPIO_ReadPin(int port, int pin) { (void)port; return button!=pin; }
static void DfPlayerMini_Stop(void) { ++stops; }
static void BuzzerPhrase400_Stop(void) { ++stops; }
static void app_audio_toggle(void) {}
static void app_audio_next(void) {}
static void app_audio_previous(void) {}
static void app_audio_adjust_volume(int step) { (void)step; }
'''
checks = r'''
_Static_assert(APP_MODE_INTEGRATED==0 && APP_MODE_LINE_ONLY==1 &&
               APP_MODE_SIGN_LINE==2 && APP_MODE_SIGN_GYRO_TANGENT==3 &&
               APP_MODE_FIXED_BYPASS==4 && APP_MODE_STOPPED==5,
               "Assigning key 4 must not renumber other mode telemetry");
int main(void)
{
  AppMode current;
  unsigned source, key;
  const uint8_t keys[]={IR_REMOTE_VIRTUAL_KEY1,IR_REMOTE_VIRTUAL_KEY2,
      IR_REMOTE_VIRTUAL_KEY3,IR_REMOTE_VIRTUAL_KEY4,IR_REMOTE_VIRTUAL_KEY5};
  const AppMode modes[]={APP_MODE_INTEGRATED,APP_MODE_LINE_ONLY,
      APP_MODE_SIGN_LINE,APP_MODE_SIGN_GYRO_TANGENT,APP_MODE_FIXED_BYPASS};
  for(current=APP_MODE_INTEGRATED;current<=APP_MODE_STOPPED;++current)
  {
    for(source=0;source<2;++source)
    {
      for(key=0;key<5;++key)
      {
        button=0; remote_input=source?0:keys[key]; serial_input=source?keys[key]:0;
        assert(read_requested_mode(current)==modes[key]);
      }
      remote_input=source?IR_REMOTE_VIRTUAL_KEY3:IR_REMOTE_VIRTUAL_KEY4;
      serial_input=source?IR_REMOTE_VIRTUAL_KEY4:IR_REMOTE_VIRTUAL_KEY3;
      button=0; stops=0;
      assert(read_requested_mode(current)==APP_MODE_SIGN_LINE); /* key 3 has priority */
    }
    remote_input=IR_REMOTE_VIRTUAL_STOP; serial_input=IR_REMOTE_VIRTUAL_KEY3;
    assert(read_requested_mode(current)==APP_MODE_STOPPED);
    remote_input=serial_input=0; button=3;
    assert(read_requested_mode(current)==APP_MODE_SIGN_LINE);
    button=0; assert(read_requested_mode(current)==current);
  }
  puts("PASS: actual selector binds standard sign to KEY3 and gyro tangent arc to KEY4; STOP and other numbers retained");
  return 0;
}
'''
build = ROOT / "manual-build-sign-line-host-test"
build.mkdir(exist_ok=True)
source = build / "test_mode_selection_extract.c"
exe = build / "test_mode_selection.exe"
source.write_text(stubs + enum.group() + "\n" + selector + "\n" + checks, encoding="utf-8")
subprocess.run(["cl", "/nologo", "/W4", "/WX", "/utf-8", "/std:c11",
                "/ICore/Inc", str(source), "/Fo" + str(build / "test_mode_selection.obj"),
                "/Fe" + str(exe)], cwd=ROOT, check=True)
subprocess.run([str(exe)], cwd=ROOT, check=True)
