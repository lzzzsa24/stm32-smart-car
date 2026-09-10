"""Verify selective promotion against immutable main and source anchors."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
baseline = '3257b05af3b8016e00f1cad772157992fc885bc6'
source = '59407274e55efd292b187e393ddcf28d0dd40474'
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
             'sign_slowdown', 'line_obstacle_bypass', 'ultrasonic_avoid',
             'motorPWM', 'wheel_encoder'):
    for folder, ext in (('Inc', 'h'), ('Src', 'c')):
        path = f'Core/{folder}/{name}.{ext}'
        assert current(path) == git_text(baseline, path), path
assert current('K210/sign_mode34.py') == git_text(baseline, 'K210/sign_mode34.py')
for name in ('line_tracking', 'line_recovery', 'sign_route', 'sign_route_config',
             'simple_line_mode', 'sign_line_follow', 'sign_observation', 'sign_horn', 'sign_trace'):
    for folder, ext in (('Inc', 'h'), ('Src', 'c')):
        if name == 'sign_route_config' and ext == 'c':
            continue
        actual = current(f'Core/{folder}/promoted_{name}.{ext}')
        actual = actual.replace('Promoted_', '').replace('promoted_', '')
        actual = actual.replace('PROMOTED_LINE_TRACKING_HEADER_H', '__LINE_TRACKING_H')
        assert actual == git_text(source, f'Core/{folder}/{name}.{ext}'), name

main = current('Core/Src/main.c')
old = git_text(baseline, 'Core/Src/main.c')
for marker in ('static void sign_line_task(AppMode mode)\n{',
               'static void apply_sign_line_pwm(int16_t left_pwm,\n',
               'static void configure_bypass_profile(uint8_t fixed)\n{',
               'static uint16_t app_emergency_distance_cm(uint32_t speed_cps)\n{',
               'static uint8_t service_legacy_line_wait(AppMode mode)\n{'):
    # Prototype may precede definition for PWM, choose its complete declaration.
    if marker.startswith('static void apply_sign'):
        marker += '                                int16_t right_pwm,\n                                uint8_t line_mask,\n                                uint8_t controller_state)\n{'
    assert block(main, marker) == block(old, marker), marker
transition = block(main, 'if (requested_mode != app_mode)')
for mode in ('APP_MODE_LINE_ONLY', 'APP_MODE_SIGN_LINE_SIMPLE'):
    assert block(transition, 'else if (app_mode == ' + mode + ')') == block(block(old, 'if (requested_mode != app_mode)'), 'else if (app_mode == ' + mode + ')')
assert 'Promoted_line_tracking_set_middle_guard(1U)' not in main
assert 'Promoted_SignRoute_SetProfile(Promoted_SIGN_ROUTE_PROFILE_STANDARD);' in main
assert 'Promoted_SIGN_ROUTE_PROFILE_GYRO_TANGENT' not in main
assert 'if (app_mode == APP_MODE_SIGN_LINE_ADVANCED) promoted_sign_task(app_mode);' in main
assert 'else sign_line_task(app_mode);' in main
assert 'Promoted_line_tracking_set_fast_follow(1U);' in block(main, 'static void experiment7_integrated_once(void)\n{')
assert 'if (fixed_bypass_mode) Promoted_line_tracking_rejoin_from_bypass(contact);' in main
assert 'Promoted_SignLineFollow_Stop(&promoted_sign_controller);' in transition
assert 'Promoted_line_tracking_reset();' in transition
print('PASS: mode1/2/4 legacy controllers and hardware unchanged; mode3/5 source identity and dispatch isolation')
