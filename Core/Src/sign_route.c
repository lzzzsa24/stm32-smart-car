#include "sign_route.h"
#include "sign_route_config.h"
#include "vehicle_geometry.h"

#include <stddef.h>
#include <string.h>

#define SIGN_WINDOW_SIZE                 5U
#define SIGN_SCORE_MINIMUM              20U
#define SIGN_WINDOW_MAX_SPAN_MS        600U
#define SIGN_ONLINE_MAX_AGE_MS         500U
#define SIGN_PENDING_MAX_AGE_MS       5000U
#define SIGN_CENTER_MAX_JUMP_PX         60U
#define SIGN_JUNCTION_CONFIRM_MS        20U
#define SIGN_REARM_COOLDOWN_MS        1500U
#define SIGN_REARM_NONE_MS             800U

typedef struct
{
  int8_t class_id[SIGN_WINDOW_SIZE];
  uint16_t center_x[SIGN_WINDOW_SIZE];
  uint16_t center_y[SIGN_WINDOW_SIZE];
  uint32_t time_ms[SIGN_WINDOW_SIZE];
  uint8_t count;
  SignRouteState state;
  SignRouteState resume_state;
  int8_t direction;
  int8_t last_class;
  uint8_t last_score;
  uint32_t last_frame_ms;
  uint32_t last_sequence;
  uint32_t armed_ms;
  uint32_t junction_since_ms;
  uint32_t capture_since_ms;
  uint32_t finished_ms;
  uint32_t none_since_ms;
  uint8_t junction_active;
  uint8_t departed;
  uint8_t capture_active;
  int32_t previous_counts[4];
  int64_t left_counts, right_counts;
  int64_t origin_left, origin_right;
  uint32_t phase_ms, last_step_ms, line_lost_ms;
  uint8_t odometry_valid, step_valid, line_lost, fault, frame_valid;
  uint8_t capture_kind;
  int32_t travel_mm, yaw_mdeg;
} SignRouteContext;

static SignRouteContext route;

static uint32_t absolute_difference(uint16_t left, uint16_t right)
{
  return left > right ? (uint32_t)(left - right) :
                        (uint32_t)(right - left);
}

static uint8_t is_junction(uint8_t mask)
{
  switch (mask & 0x0FU)
  {
    case 0U:
    case 1U:
    case 2U:
    case 3U:
    case 4U:
    case 6U:
    case 8U:
    case 12U:
      return 0U;
    default:
      return 1U;
  }
}

static uint8_t is_center_line(uint8_t mask)
{
  mask &= 0x0FU;
  return mask == 2U || mask == 4U || mask == 6U;
}

static void clear_window(void)
{
  route.count = 0U;
}

static void remove_stale(uint32_t now)
{
  uint8_t remove = 0U;
  uint8_t i;

  while (remove < route.count &&
         now - route.time_ms[remove] > SIGN_WINDOW_MAX_SPAN_MS)
  {
    ++remove;
  }
  if (remove == 0U)
  {
    return;
  }
  for (i = remove; i < route.count; ++i)
  {
    route.class_id[i - remove] = route.class_id[i];
    route.center_x[i - remove] = route.center_x[i];
    route.center_y[i - remove] = route.center_y[i];
    route.time_ms[i - remove] = route.time_ms[i];
  }
  route.count = (uint8_t)(route.count - remove);
}

static void append_observation(int8_t class_id,
                               uint16_t center_x,
                               uint16_t center_y,
                               uint32_t now)
{
  uint8_t i;

  remove_stale(now);
  if (route.count >= SIGN_WINDOW_SIZE)
  {
    for (i = 1U; i < SIGN_WINDOW_SIZE; ++i)
    {
      route.class_id[i - 1U] = route.class_id[i];
      route.center_x[i - 1U] = route.center_x[i];
      route.center_y[i - 1U] = route.center_y[i];
      route.time_ms[i - 1U] = route.time_ms[i];
    }
    route.count = SIGN_WINDOW_SIZE - 1U;
  }
  route.class_id[route.count] = class_id;
  route.center_x[route.count] = center_x;
  route.center_y[route.count] = center_y;
  route.time_ms[route.count] = now;
  ++route.count;
}

static void try_confirm(uint32_t now)
{
  int8_t candidate;
  uint8_t votes = 0U;
  uint8_t i;

  if ((route.state != SIGN_ROUTE_IDLE && route.state != SIGN_ROUTE_PROBE &&
       route.state != SIGN_ROUTE_WAIT_SIGN) || route.count < 3U)
  {
    return;
  }
  candidate = route.class_id[route.count - 1U];
  if ((candidate != 0 && candidate != 1) ||
      route.class_id[route.count - 2U] != candidate ||
      now - route.time_ms[route.count - 1U] > 350U)
  {
    return;
  }
  for (i = 0U; i < route.count; ++i)
  {
    if (route.class_id[i] == candidate)
    {
      ++votes;
    }
  }
  if (votes >= 3U)
  {
    route.direction = candidate == 0 ? -1 : 1;
    if (route.state == SIGN_ROUTE_IDLE) route.state = SIGN_ROUTE_ARMED;
    route.armed_ms = now;
    route.junction_active = 0U;
  }
}

void SignRoute_Init(void)
{
  SignRoute_Reset();
}

void SignRoute_Reset(void)
{
  memset(&route, 0, sizeof(route));
  route.last_class = -1;
}

void SignRoute_ObserveDetection(const VisionDetection *detection)
{
  int8_t candidate;
  uint32_t now;

  if (detection == NULL ||
      (route.frame_valid && detection->sequence == route.last_sequence))
  {
    return;
  }
  if (route.frame_valid &&
      detection->sequence != route.last_sequence + 1U)
  {
    clear_window();
  }
  if (route.frame_valid && detection->received_ms - route.last_frame_ms > SIGN_ONLINE_MAX_AGE_MS)
    route.none_since_ms = 0U;
  route.frame_valid = 1U;
  route.last_sequence = detection->sequence;
  route.last_frame_ms = detection->received_ms;
  route.last_class = detection->class_id;
  route.last_score = detection->score;
  now = detection->received_ms;
  candidate = (detection->score >= SIGN_SCORE_MINIMUM &&
               (detection->class_id == 0 || detection->class_id == 1)) ?
              detection->class_id : -1;

  if (candidate >= 0 && route.count != 0U &&
      route.class_id[route.count - 1U] == candidate &&
      (absolute_difference(route.center_x[route.count - 1U],
                           detection->center_x) > SIGN_CENTER_MAX_JUMP_PX ||
       absolute_difference(route.center_y[route.count - 1U],
                           detection->center_y) > SIGN_CENTER_MAX_JUMP_PX))
  {
    clear_window();
  }
  append_observation(candidate, detection->center_x,
                     detection->center_y, now);

  if (detection->class_id == -1)
  {
    if (route.none_since_ms == 0U)
    {
      route.none_since_ms = now;
    }
  }
  else
  {
    route.none_since_ms = 0U;
  }
  try_confirm(now);
}

void SignRoute_UpdateEncoders(int32_t m1, int32_t m2, int32_t m3, int32_t m4)
{
  int32_t current[4] = {m1, m2, m3, m4};
  int32_t delta[4] = {0};
  uint8_t i;
  for (i = 0U; i < 4U; ++i)
  {
    if (route.odometry_valid)
      delta[i] = (int32_t)((uint32_t)current[i] - (uint32_t)route.previous_counts[i]);
    route.previous_counts[i] = current[i];
  }
  route.odometry_valid = 1U;
  route.left_counts += (int64_t)delta[0] + delta[1];
  route.right_counts += (int64_t)delta[2] + delta[3];
}

static void enter_phase(SignRouteState state, uint32_t now)
{
  route.state = state;
  route.phase_ms = now;
  route.origin_left = route.left_counts;
  route.origin_right = route.right_counts;
  route.capture_active = route.departed = route.line_lost = 0U;
  route.fault = 0U;
  route.travel_mm = route.yaw_mdeg = 0L;
}

static void update_geometry(void)
{
  int64_t left_mm = (route.left_counts - route.origin_left) *
      VEHICLE_WHEEL_DIAMETER_MM * 31416LL / (2080LL * 10000LL);
  int64_t right_mm = (route.right_counts - route.origin_right) *
      VEHICLE_WHEEL_DIAMETER_MM * 31416LL / (2080LL * 10000LL);
  route.travel_mm = (int32_t)((left_mm + right_mm) / 2LL);
  route.yaw_mdeg = (int32_t)((right_mm - left_mm) * 180000LL * 10000LL /
      (VEHICLE_TRACK_WIDTH_MM * 31416LL));
}

/* A gap cannot stand in for repeated sensor observations. */
static uint8_t stable(uint8_t condition, uint32_t now)
{
  if (!condition) { route.capture_active = 0U; return 0U; }
  if (!route.capture_active || route.capture_kind != condition)
  {
    route.capture_active = 1U;
    route.capture_kind = condition;
    route.capture_since_ms = now;
    return 0U;
  }
  return now - route.capture_since_ms >= SIGN_CAPTURE_MS;
}

static void fail_route(uint8_t reason, SignRouteCommand *command)
{
  if (route.state != SIGN_ROUTE_FAULT) route.resume_state = route.state;
  route.state = SIGN_ROUTE_FAULT;
  route.fault = reason;
  command->active = 1U;
  command->left_pwm = command->right_pwm = 0;
}

static void select_command(SignRouteCommand *command, uint8_t mask)
{
  uint8_t selected_edge = route.direction < 0 ? 8U : 1U;
  /* A sign is only a branch preference. It cannot drive off the black line,
     override a current centre line, or steer across an all-black bar. */
  if (route.direction == 0 || !(mask & selected_edge) || mask == 15U)
    return;
  command->active = 1U;
  /* Forward pivot, never equal-and-opposite wheel rotation. Exit uses the
     SAME side as entry: a right semicircle exits to the right of its tangent. */
  command->left_pwm = route.direction < 0 ? 0 : SIGN_ROUTE_PWM;
  command->right_pwm = route.direction < 0 ? SIGN_ROUTE_PWM : 0;
}

void SignRoute_Step(uint8_t line_mask, uint32_t now, SignRouteCommand *command)
{
  uint8_t junction, center;
  int32_t directed_arc_yaw;
  if (command == NULL) return;
  memset(command, 0, sizeof(*command));
  command->direction = route.direction;
  line_mask &= 15U;
  junction = is_junction(line_mask);
  center = is_center_line(line_mask);
  if (route.step_valid && now - route.last_step_ms > SIGN_SAMPLE_MAX_GAP_MS)
  {
    route.capture_active = 0U;
    route.junction_active = 0U;
  }
  route.step_valid = 1U;
  route.last_step_ms = now;
  update_geometry();

  if (route.state == SIGN_ROUTE_FAULT)
  {
    if (!stable(line_mask != 0U, now))
    {
      fail_route(route.fault, command);
      return;
    }
    /* Only actual stable line evidence resumes a line-loss stop; a fresh
       visual detection alone cannot start the vehicle. */
    route.state = route.resume_state;
    route.fault = route.line_lost = route.capture_active = 0U;
  }
  if (line_mask == 0U)
  {
    if (!route.line_lost) { route.line_lost = 1U; route.line_lost_ms = now; }
    if (now - route.line_lost_ms >= SIGN_LINE_LOST_TIMEOUT_MS)
    {
      route.capture_active = 0U;
      fail_route(4U, command);
      return;
    }
  }
  else route.line_lost = 0U;
  if ((route.state == SIGN_ROUTE_ARMED || route.state == SIGN_ROUTE_PROBE) &&
      route.direction != 0 &&
      (now - route.armed_ms > SIGN_PENDING_MAX_AGE_MS ||
       now - route.last_frame_ms > SIGN_ONLINE_MAX_AGE_MS))
  {
    route.direction = 0;
    clear_window();
    if (route.state == SIGN_ROUTE_ARMED) route.state = SIGN_ROUTE_IDLE;
  }
  if (route.state == SIGN_ROUTE_LOCKED)
  {
    if (now - route.finished_ms >= SIGN_REARM_COOLDOWN_MS &&
        route.none_since_ms != 0U &&
        now - route.none_since_ms >= SIGN_REARM_NONE_MS &&
        now - route.last_frame_ms <= SIGN_ONLINE_MAX_AGE_MS &&
        route.last_class == -1)
    {
      route.state = SIGN_ROUTE_IDLE;
      route.direction = 0;
      route.junction_active = 0U;
      clear_window();
    }
    return;
  }

  if (route.state == SIGN_ROUTE_IDLE || route.state == SIGN_ROUTE_ARMED)
  {
    if (!junction) { route.junction_active = 0U; return; }
    if (!route.junction_active)
    {
      route.junction_active = 1U;
      route.junction_since_ms = now;
      return;
    }
    if (now - route.junction_since_ms < SIGN_JUNCTION_CONFIRM_MS) return;
    enter_phase(SIGN_ROUTE_PROBE, now);
    command->just_started = 1U;
  }

  if (route.state == SIGN_ROUTE_PROBE)
  {
    /* Observe while the ordinary line controller keeps motor ownership. */
    if (route.travel_mm > SIGN_PROBE_MAX_MM ||
        now - route.phase_ms > SIGN_PROBE_TIMEOUT_MS)
    {
      route.fault = 1U; /* navigation warning, never an on-line stop */
      route.state = route.direction ? SIGN_ROUTE_ARMED : SIGN_ROUTE_IDLE;
      route.junction_active = route.capture_active = 0U;
      return;
    }
    /* Cross the transverse stroke first. A continuing middle line is a
       painted crossbar, not a command to turn into the circle. */
    if (stable((uint8_t)(!junction ? (center ? 1U : 2U) : 0U), now))
    {
      if (center)
      {
        route.state = route.direction ? SIGN_ROUTE_ARMED : SIGN_ROUTE_IDLE;
        route.junction_active = route.capture_active = 0U;
        command->just_finished = 1U;
      }
      else if (route.direction != 0)
      {
        enter_phase(SIGN_ROUTE_SELECTING, now);
        route.departed = 1U;
        select_command(command, line_mask);
      }
      else
      {
        enter_phase(SIGN_ROUTE_WAIT_SIGN, now);
        clear_window();
      }
    }
    return;
  }

  if (route.state == SIGN_ROUTE_WAIT_SIGN)
  {
    /* Missing/incompatible K210 frames do not disable four-sensor tracking. */
    if (center)
    {
      route.state = route.direction ? SIGN_ROUTE_ARMED : SIGN_ROUTE_IDLE;
      route.capture_active = route.junction_active = 0U;
      return;
    }
    if (route.direction == 0) return;
    enter_phase(SIGN_ROUTE_SELECTING, now);
    route.departed = 1U;
    command->just_started = 1U;
  }

  if (route.state == SIGN_ROUTE_SELECTING || route.state == SIGN_ROUTE_EXIT_SELECT)
  {
    int32_t turn_yaw = -route.direction * route.yaw_mdeg;
    select_command(command, line_mask);
    if (now - route.phase_ms > SIGN_SELECT_TIMEOUT_MS ||
        turn_yaw > SIGN_SELECT_MAX_YAW_MDEG || turn_yaw < -30000L)
      route.fault = 2U; /* encoder/time assumptions cannot suppress a live line */
    if (!junction && !center) route.departed = 1U;
    if (stable(route.departed && center, now))
    {
      if (route.state == SIGN_ROUTE_SELECTING)
      {
        enter_phase(SIGN_ROUTE_ARC, now);
        command->active = 0U;
      }
      else
      {
        enter_phase(SIGN_ROUTE_EXIT_CLEAR, now);
      }
      command->just_finished = 1U;
    }
    return;
  }

  if (route.state == SIGN_ROUTE_ARC)
  {
    uint8_t exit_side = route.direction < 0 ? 8U : 1U;
    directed_arc_yaw = route.direction * route.yaw_mdeg;
    if (now - route.phase_ms > SIGN_ARC_TIMEOUT_MS ||
        route.travel_mm > SIGN_ARC_MAX_MM ||
        directed_arc_yaw > SIGN_ARC_MAX_YAW_MDEG ||
        directed_arc_yaw < -90000L)
      route.fault = 3U;
    /* Curvature toward the circle is opposite the selected entry side.
       The outgoing line appears on the outside after substantial arc travel. */
    if (stable(((route.travel_mm >= SIGN_ARC_MIN_MM &&
                 directed_arc_yaw >= SIGN_ARC_MIN_YAW_MDEG) || route.fault == 3U) &&
               (line_mask & exit_side) && (line_mask & 6U), now))
    {
      enter_phase(SIGN_ROUTE_EXIT_SELECT, now);
      route.departed = 1U;
      command->just_started = 1U;
      select_command(command, line_mask);
    }
    return;
  }

  if (route.state == SIGN_ROUTE_EXIT_CLEAR)
  {
    if (now - route.phase_ms > SIGN_EXIT_CLEAR_TIMEOUT_MS)
      route.fault = 5U;
    if (stable(center && (route.travel_mm >= SIGN_EXIT_CLEAR_MM || route.fault == 5U), now))
    {
      route.state = SIGN_ROUTE_LOCKED;
      route.finished_ms = now;
      route.none_since_ms = 0U; /* only post-exit absence permits rearming */
      command->active = 0U;
      command->just_finished = 1U;
    }
  }
  command->direction = route.direction;
}

void SignRoute_GetStatus(uint32_t now, SignRouteStatus *status)
{
  if (status == NULL) return;
  status->state = route.state;
  status->direction = route.direction;
  status->last_class = route.last_class;
  status->last_score = route.last_score;
  status->vision_online = route.frame_valid &&
      now - route.last_frame_ms <= SIGN_ONLINE_MAX_AGE_MS ? 1U : 0U;
  status->last_sequence = route.last_sequence;
  status->fault = route.fault;
  status->travel_mm = route.travel_mm;
  status->yaw_mdeg = route.yaw_mdeg;
}
