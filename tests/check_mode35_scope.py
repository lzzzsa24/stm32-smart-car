"""Verify selective promotion against immutable main and source anchors."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
baseline = '3257b05af3b8016e00f1cad772157992fc885bc6'
source = 'a217df81e807f54a7b95fc8c8541876cdc59bfc8'
def git_text(ref, path):
    return subprocess.check_output(['git', 'show', ref + ':' + path], cwd=root).decode('utf-8').replace('\r\n', '\n').rstrip()
def current(path):
    return (root / path).read_text(encoding='utf-8').rstrip()
def block(text, marker):
    start = text.index(marker)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

for name in ('line_tracking', 'line_recovery', 'sign_route', 'simple_line_mode',
             'sign_slowdown', 'ultrasonic_avoid',
             'motorPWM', 'wheel_encoder'):
    for folder, ext in (('Inc', 'h'), ('Src', 'c')):
        path = f'Core/{folder}/{name}.{ext}'
        assert current(path) == git_text(baseline, path), path
assert current('K210/sign_mode34.py') == git_text(source, 'K210/sign_mode34.py')
expected_bypass = git_text(source, 'Core/Src/line_obstacle_bypass.c')
expected_bypass = expected_bypass.replace('  config->infrared_enabled = 1U;',
    '  config->infrared_enabled = 1U;\n  config->adaptive_return_max_cps = 1800U;')
expected_bypass = expected_bypass.replace('fixed_phase == LINE_FIXED_RETURN ? 4000L : 2100L',
    'fixed_phase == LINE_FIXED_RETURN ? 4000L : bypass_config.adaptive_return_max_cps')
assert current('Core/Src/line_obstacle_bypass.c') == expected_bypass
assert current('Core/Inc/line_obstacle_bypass.h') == git_text(baseline, 'Core/Inc/line_obstacle_bypass.h')
for name in ('line_tracking', 'line_recovery', 'sign_route', 'sign_route_config',
             'simple_line_mode', 'sign_line_follow', 'sign_observation', 'sign_horn', 'sign_trace'):
    for folder, ext in (('Inc', 'h'), ('Src', 'c')):
        if name == 'sign_route_config' and ext == 'c':
            continue
        actual = current(f'Core/{folder}/promoted_{name}.{ext}')
        actual = actual.replace('Promoted_', '').replace('promoted_', '')
        actual = actual.replace('PROMOTED_LINE_TRACKING_HEADER_H', '__LINE_TRACKING_H')
        expected = git_text(source, f'Core/{folder}/{name}.{ext}')
        # Explicit mode3-only exceptions to mechanical promotion. Functional
        # route/DriveBase regressions verify these edited functions; all other
        # modules and shared mode1/2/4 paths remain byte-identical to anchors.
        if name == 'sign_route' and ext == 'h':
            actual = actual.replace('  uint8_t heading_drive;  /* mode3 EXIT LINE: forward CPS with gyro correction */\n', '')
            actual = actual.replace('  int32_t drive_heading_error_mdeg; /* relative to EXIT LINE entry, not the stop */\n', '')
        if name == 'sign_route' and ext == 'c':
            actual = actual.replace('  uint8_t exit_started;\n  uint32_t exit_started_ms;', '  uint8_t exit_line_lost;')
            actual = actual.replace('  int64_t exit_drive_yaw;\n', '')
            actual = actual.replace(block(actual, 'static void exit_line_command(') + '\n\n', '')
            for marker in ('static void enter_phase(SignRouteState state, uint32_t now)\n{',
                           'static void cancel_route(', 'static void complete_route(',
                           'void SignRoute_Step('):
                actual = actual.replace(block(actual, marker), block(expected, marker))
        if name == 'sign_line_follow' and ext == 'c':
            actual = actual.replace(block(actual, '      if (mode3 && route_command->heading_drive)') + '\n      else if', '      if')
            actual = actual.replace('    /* Ordinary following/search resumes when route ownership is released. */',
                "    /* Mode 3's aligned exit is already live tracking, with the same slow\n"
                '       targets. An old crossing tail must not hide a current outer contact. */')
        assert actual == expected, name

main = current('Core/Src/main.c')
old = git_text(baseline, 'Core/Src/main.c')
# Preserve mode1/2 shared drivers and critical application paths against pre-change main.
old = git_text('36f4aed', 'Core/Src/main.c')
for marker in ('static void configure_bypass_profile(uint8_t fixed)\n{',
               'static uint16_t app_emergency_distance_cm(uint32_t speed_cps)\n{',
               'static uint8_t service_legacy_line_wait(AppMode mode)\n{',
               'static uint8_t service_bounded_line_wait(AppMode mode)\n{',
               'static void experiment7_integrated_once(void)\n{'):
    assert block(main, marker) == block(old, marker), marker
transition = block(main, 'if (requested_mode != app_mode)')
assert block(transition, 'else if (app_mode == APP_MODE_LINE_ONLY)') == block(block(old, 'if (requested_mode != app_mode)'), 'else if (app_mode == APP_MODE_LINE_ONLY)')
for name in ('drive_base', 'line_bypass_turn', 'line_bypass_travel',
             'ir_avoid', 'ultrasonic', 'line_sensor_sample'):
    for folder, ext in (('Inc','h'), ('Src','c')):
        path = f'Core/{folder}/{name}.{ext}'
        assert current(path) == git_text('36f4aed', path), path
assert 'APP_MODE_SIGN_LINE_SIMPLE' not in main
assert '5=FIXED' not in main
assert 'Promoted_line_tracking_set_middle_guard(1U)' not in main
assert 'Promoted_SignRoute_SetProfile(Promoted_SIGN_ROUTE_PROFILE_STANDARD);' in main
assert 'Promoted_SIGN_ROUTE_PROFILE_GYRO_TANGENT' not in main
assert 'promoted_sign_task(app_mode);' in main
assert 'else sign_line_task(app_mode);' not in main
assert 'Promoted_line_tracking_set_fast_follow(1U);' in block(main, 'static void experiment7_integrated_once(void)\n{')
assert 'if (fixed_bypass_mode) Promoted_line_tracking_rejoin_from_bypass(contact);' in main
assert 'Promoted_SignLineFollow_Stop(&promoted_sign_controller);' in transition
assert 'Promoted_line_tracking_reset();' in transition
print('PASS: mode1/2 legacy controllers/hardware unchanged; bounded mode3 exit exceptions, remaining promoted source identity and four-mode dispatch isolation')
