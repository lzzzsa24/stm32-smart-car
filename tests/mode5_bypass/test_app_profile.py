"""Compile actual app profile and emergency calculation; verify switching isolation."""
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
main = (ROOT / 'Core/Src/main.c').read_text(encoding='utf-8')
def function(name):
    start = main.index(name + '\n{')
    opening = main.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (main[end] == '{') - (main[end] == '}')
        end += 1
    return main[start:end]

assert 'vision_line_v4_task' not in main and 'VisionLineV4Control_Step' not in main
assert 'configure_bypass_profile(app_mode == APP_MODE_FIXED_BYPASS);' in main
assert 'app_mode == APP_MODE_LINE_ONLY || app_mode == APP_MODE_FIXED_BYPASS' in main
assert 'line_tracking_set_straight_boost(fixed_bypass_mode);' in main
assert 'if (!fixed_bypass_mode && confirmed_ir_bypass_direction' in main
assert 'LineBypassRange_Task(' in main
assert main.index('LineObstacleBypass_Stop();', main.index('if (requested_mode != app_mode)')) < main.index('configure_bypass_profile(app_mode')
assert 'fixed_bypass_mode ? 10U : 5U' in function('static void configure_ultrasonic_avoid(void)')
# Existing sign route/controller/driver and camera sources must not be promoted.
for path in ('Core/Src/sign_route.c', 'Core/Src/simple_line_mode.c',
             'Core/Src/sign_slowdown.c', 'Core/Src/drive_base.c',
             'Core/Src/motorPWM.c', 'Core/Src/wheel_encoder.c', 'K210/sign_mode34.py'):
    expected = subprocess.check_output(['git', 'show', 'e643c39:' + path], cwd=ROOT)
    actual = (ROOT / path).read_bytes()
    assert actual.replace(b'\r\n', b'\n') == expected.replace(b'\r\n', b'\n'), path

macros = '\n'.join(re.findall(r'^#define EXP7_[^\n]+', main, re.M))
stubs = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "line_obstacle_bypass.h"
#define IR_LEFT_ENABLE_GPIO_Port 0
#define IR_RIGHT_ENABLE_GPIO_Port 1
#define IR_LEFT_ENABLE_Pin 5
#define IR_RIGHT_ENABLE_Pin 6
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
static uint8_t fixed_bypass_mode, legacy_ir_enabled, ir_on=1;
static uint16_t active_stop_cm, active_clear_cm, active_emergency_max_cm, active_lookahead_ms;
static int pins[2], resets;
static LineObstacleBypassConfig applied;
static uint8_t ir_avoid_is_enabled(void) { return ir_on; }
static void ir_avoid_set_enabled(int on) { ir_on=(uint8_t)on; }
static void HAL_GPIO_WritePin(int port,int pin,int value) { (void)pin;pins[port]=value; }
void LineObstacleBypass_GetDefaultConfig(LineObstacleBypassConfig *c) { memset(c,0,sizeof(*c)); }
void LineObstacleBypass_Init(const LineObstacleBypassConfig *c) { applied=*c; }
static void LineBypassRange_Reset(void) { ++resets; }
'''
checks = r'''
int main(void) {
  for(int i=0;i<50;i++) {
    configure_bypass_profile(1);
    assert(fixed_bypass_mode && !ir_on && pins[0]==1 && pins[1]==1);
    assert(applied.fixed_route_direction==1 && !applied.infrared_enabled);
    assert(applied.forward_cps==4000 && applied.return_cps==4000);
    assert(applied.adaptive_return_max_cps==2100);
    assert(active_stop_cm==16 && active_clear_cm==28 && active_lookahead_ms==100);
    assert(app_emergency_distance_cm(0)==16 && app_emergency_distance_cm(5300)==22);
    configure_bypass_profile(0);
    assert(!fixed_bypass_mode && ir_on && pins[0]==0 && pins[1]==0);
    assert(applied.fixed_route_direction==0 && applied.infrared_enabled);
    assert(applied.forward_cps==2600 && applied.return_cps==2300);
    assert(applied.adaptive_return_max_cps==1800);
    assert(active_stop_cm==10 && active_clear_cm==18 && active_lookahead_ms==70);
    assert(app_emergency_distance_cm(0)==10 && app_emergency_distance_cm(5300)==16);
  }
  ir_on=0; configure_bypass_profile(1); configure_bypass_profile(0);
  assert(!ir_on); /* failed legacy calibration must not be re-enabled */
  assert(resets==102);
  puts("PASS: actual mode5/legacy profile switching, thresholds, IR restore and unchanged sign/driver sources");
  return 0;
}
'''
build = ROOT / 'manual-build-mode5-host-test'
build.mkdir(exist_ok=True)
source = build / 'app_profile.c'
source.write_text(macros + '\n' + stubs + '\n' + function('static uint16_t app_emergency_distance_cm(uint32_t speed_cps)') + '\n' + function('static void configure_bypass_profile(uint8_t fixed)') + '\n' + checks, encoding='utf-8')
exe=build/'app_profile.exe'
subprocess.run(['cl','/nologo','/W4','/WX','/utf-8','/std:c11','/ICore/Inc',str(source),'/Fo'+str(build/'app_profile.obj'),'/Fe'+str(exe)],cwd=ROOT,check=True)
subprocess.run([str(exe)],cwd=ROOT,check=True)
