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
  int8_t direction;
  int8_t last_class;
  uint8_t last_score;
  uint32_t last_frame_ms;
  uint32_t last_sequence;
  uint32_t armed_ms;
  int64_t imu_yaw, imu_origin, approach_yaw;
  int64_t entry_extreme_yaw, exit_previous_error;
  uint8_t arc_origin_locked;
  uint8_t imu_valid;
  uint32_t probe_hold_since_ms;
  uint8_t probe_hold_started;
  uint32_t junction_since_ms;
  uint32_t capture_since_ms;
  uint32_t finished_ms;
  uint32_t none_since_ms;
  uint8_t junction_active;
  uint8_t departed;
  uint8_t entry_edge_seen, entry_center_active, entry_line_ready;
  uint8_t exit_region_seen;
  uint8_t exit_line_lost;
  uint8_t arc_lower_seen, exit_straight_active;
  uint32_t exit_straight_since_ms;
  int64_t exit_straight_origin_counts;
  int32_t exit_straight_min_yaw, exit_straight_max_yaw, exit_heading_peak_mdeg;
  uint32_t entry_center_ms;
  uint8_t capture_active;
  int32_t previous_counts[4];
  int64_t left_counts, right_counts;
  int64_t origin_left, origin_right;
  uint32_t phase_ms, last_step_ms;
  uint8_t odometry_valid, step_valid, fault, frame_valid, last_line_mask;
  uint8_t capture_kind;
  uint8_t observation_pause_active, observation_pause_seen, fallback_arc_hint;
  uint8_t observation_search_seen;
  uint32_t observation_pause_since_ms;
  uint8_t approach_from_pause;
  uint32_t pause_reference_ms;
  int32_t travel_mm, yaw_mdeg;
  SignRouteProfile profile;
  int32_t arc_peak_mdeg;
  int32_t exit_best_error_mdeg;
} SignRouteContext;

static SignRouteContext route;

static void enter_phase(SignRouteState state, uint32_t now);

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

static uint8_t is_track_line(uint8_t mask)
{
  return mask != 0U && !is_junction(mask);
}

static int32_t heading_error(void)
{
  int64_t error=(route.imu_yaw-route.approach_yaw)%360000LL;
  if(error>180000LL) error-=360000LL;
  if(error<-180000LL) error+=360000LL;
  return (int32_t)error;
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
       route.state != SIGN_ROUTE_WAIT_SIGN) || route.direction != 0 || route.count < 3U)
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
  SignRouteProfile profile = route.profile;
  memset(&route, 0, sizeof(route));
  route.profile = profile;
  route.last_class = -1;
}

void SignRoute_SetProfile(SignRouteProfile profile)
{
  route.profile = profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT ?
      SIGN_ROUTE_PROFILE_GYRO_TANGENT : SIGN_ROUTE_PROFILE_STANDARD;
}

void SignRoute_MarkObservationSearch(void)
{
  if (route.profile==SIGN_ROUTE_PROFILE_STANDARD) route.observation_search_seen=1U;
}

void SignRoute_UpdateObservationPause(uint8_t paused, uint32_t now)
{
  paused = paused ? 1U : 0U;
  if (paused && !route.observation_pause_active)
    route.observation_pause_since_ms=now;
  if (!paused && route.observation_pause_active &&
      route.profile == SIGN_ROUTE_PROFILE_STANDARD)
  {
    /* Recognition time is not elapsed driving/search time. Restart short
       sensor confirmations from a live moving sample; preserve the choice. */
    uint32_t stopped_ms=now-route.observation_pause_since_ms;
    route.phase_ms+=stopped_ms;
    if (route.probe_hold_started) route.probe_hold_since_ms+=stopped_ms;
  }
  if (route.profile != SIGN_ROUTE_PROFILE_GYRO_TANGENT)
  {
    /* The caller supplies the actual fixed-two-second observation flag after
       refreshing MPU yaw. Freeze the latest heading on its falling edge;
       do not confuse the later crossbar correction with the straight road. */
    if (paused && !route.observation_pause_active) route.approach_from_pause=0U;
    if (!paused && route.observation_pause_active && route.imu_valid &&
        (route.state==SIGN_ROUTE_IDLE || route.state==SIGN_ROUTE_ARMED ||
         route.state==SIGN_ROUTE_PROBE || route.state==SIGN_ROUTE_WAIT_SIGN ||
         route.state==SIGN_ROUTE_SELECTING))
    {
      route.approach_yaw=route.imu_yaw;
      route.approach_from_pause=1U;
      route.pause_reference_ms=now;
      if (route.observation_search_seen)
      {
        /* Centering yaw is not entry-turn progress. Keep the sign but start
           pre-entry geometry from the newly centered observation-stop pose. */
        enter_phase(route.state,now);
        if (route.direction) route.armed_ms=now;
      }
    }
    if (!paused) route.observation_search_seen=0U;
    route.observation_pause_active = paused;
    route.observation_pause_seen = 0U;
    return;
  }
  if (paused)
  {
    route.observation_pause_seen = 1U;
  }
  else if (route.observation_pause_active && route.observation_pause_seen &&
           route.state == SIGN_ROUTE_ARMED && route.direction != 0)
  {
    /* The marked stop point in the supplied trajectory is the spatial handoff:
       after recognition finishes, begin the fixed-angle entry turn here. */
    enter_phase(SIGN_ROUTE_PROBE, now);
    route.observation_pause_seen = 0U;
  }
  route.observation_pause_active = paused;
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

void SignRoute_UpdateYaw(int64_t yaw_mdeg, uint8_t valid)
{
  route.imu_yaw = yaw_mdeg;
  route.imu_valid = valid;
}

static void enter_phase(SignRouteState state, uint32_t now)
{
  if (state == SIGN_ROUTE_PROBE)
  {
    route.probe_hold_started = 0U;
    if (route.profile != SIGN_ROUTE_PROFILE_STANDARD || !route.approach_from_pause ||
        now-route.pause_reference_ms > SIGN_PENDING_MAX_AGE_MS)
    {
      route.approach_yaw = route.imu_yaw; /* no usable observation stop: explicit fallback */
      route.approach_from_pause=0U;
    }
    route.fallback_arc_hint = 0U;
  }
  route.state = state;
  route.phase_ms = now;
  route.origin_left = route.left_counts;
  route.origin_right = route.right_counts;
  route.imu_origin = route.imu_yaw;
  if (state == SIGN_ROUTE_PROBE || state == SIGN_ROUTE_SELECTING)
    route.entry_extreme_yaw = route.imu_yaw;
  if (state == SIGN_ROUTE_ARC)
  {
    route.arc_peak_mdeg = 0L;
    route.exit_region_seen = 0U;
    route.exit_heading_peak_mdeg = 0L;
    route.exit_straight_active = 0U;
    route.arc_lower_seen = route.imu_valid && is_track_line(route.last_line_mask) &&
        -route.direction * heading_error() >= 15000L;
    if (route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT)
    {
      /* The drawn path starts the arc only after the straight diagonal has
         recaptured it. Measure arc yaw from that physical contact. */
      route.imu_origin = route.imu_yaw;
      route.arc_origin_locked = 1U;
    }
    else
    {
      route.imu_origin = route.entry_extreme_yaw;
      route.arc_origin_locked = 0U;
    }
  }
  if (state == SIGN_ROUTE_EXIT_SELECT)
  {
    route.exit_line_lost=0U;
    route.exit_previous_error = route.direction * (route.profile == SIGN_ROUTE_PROFILE_STANDARD ?
        heading_error() : route.imu_yaw-route.approach_yaw);
    route.exit_best_error_mdeg = heading_error();
    if (route.exit_best_error_mdeg < 0) route.exit_best_error_mdeg = -route.exit_best_error_mdeg;
  }
  route.capture_active = route.departed = 0U;
  route.entry_edge_seen = route.entry_center_active = route.entry_line_ready = 0U;
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
#if SIGN_ROUTE_REQUIRE_IMU
  {
    int64_t yaw;
    if (route.imu_valid && route.direction)
    {
      if ((route.state == SIGN_ROUTE_PROBE || route.state == SIGN_ROUTE_SELECTING) &&
          -route.direction * (route.imu_yaw-route.entry_extreme_yaw) > 0)
        route.entry_extreme_yaw=route.imu_yaw;
      if (route.state == SIGN_ROUTE_ARC && !route.arc_origin_locked)
      {
        /* Early line capture can precede the entry turn's apex. Track that
           apex only until actual opposite arc curvature is established. */
        if (route.direction * (route.imu_yaw-route.imu_origin) < 0 &&
            -route.direction * (route.imu_yaw-route.approach_yaw) <= SIGN_ENTRY_MAX_MDEG)
          route.imu_origin=route.imu_yaw;
        if ((route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT || is_track_line(route.last_line_mask)) &&
            route.travel_mm >= SIGN_ARC_MIN_MM &&
            route.direction * (route.imu_yaw-route.imu_origin) >= SIGN_ARC_ORIGIN_LOCK_MDEG)
          route.arc_origin_locked=1U;
      }
    }
    yaw = route.imu_yaw - route.imu_origin;
    route.yaw_mdeg = yaw > 1000000 ? 1000000 :
        (yaw < -1000000 ? -1000000 : (int32_t)yaw);
  }
#endif
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

static void cancel_route(uint8_t reason, uint32_t now, SignRouteCommand *command)
{
  route.state = SIGN_ROUTE_CANCELLED;
  route.fault = reason;
  route.direction = 0;
  route.finished_ms = now;
  route.approach_from_pause=0U;
  route.none_since_ms = 0U;
  route.capture_active = route.junction_active = 0U;
  clear_window();
  memset(command, 0, sizeof(*command)); /* withdraw, never replace SL2 with STOP */
}

static void complete_route(uint32_t now, SignRouteCommand *command)
{
  route.state=SIGN_ROUTE_LOCKED;
  route.direction=0;
  route.finished_ms=now;
  route.approach_from_pause=0U;
  route.none_since_ms=0U;
  route.capture_active=route.junction_active=0U;
  route.exit_straight_active=0U;
  clear_window();
  memset(command,0,sizeof(*command));
  command->just_finished=1U;
}

#if SIGN_ROUTE_REQUIRE_IMU
static uint8_t natural_departure_confirmed(uint8_t mask, int32_t road_heading, uint32_t now)
{
  int64_t forward_mm;
  uint8_t returning=route.arc_lower_seen && is_track_line(mask) &&
      route.exit_heading_peak_mdeg >= SIGN_EXIT_RETURN_PEAK_MDEG &&
      route.exit_heading_peak_mdeg-road_heading >= SIGN_EXIT_RETURN_DROP_MDEG &&
      road_heading >= -SIGN_EXIT_RETURN_RANGE_MDEG && road_heading <= SIGN_EXIT_RETURN_RANGE_MDEG;
  if (!returning) { route.exit_straight_active=0U; return 0U; }
  if (road_heading < route.exit_straight_min_yaw) route.exit_straight_min_yaw=road_heading;
  if (road_heading > route.exit_straight_max_yaw) route.exit_straight_max_yaw=road_heading;
  if (!route.exit_straight_active ||
      route.exit_straight_max_yaw-route.exit_straight_min_yaw > SIGN_EXIT_STEADY_RANGE_MDEG)
  {
    route.exit_straight_active=1U;
    route.exit_straight_since_ms=now;
    route.exit_straight_origin_counts=route.left_counts+route.right_counts;
    route.exit_straight_min_yaw=route.exit_straight_max_yaw=road_heading;
    return 0U;
  }
  forward_mm=(route.left_counts+route.right_counts-route.exit_straight_origin_counts) *
      VEHICLE_WHEEL_DIAMETER_MM * 31416LL / (4160LL * 10000LL);
  /* Current line owns every sample during confirmation. Time alone, stationary
     search rotation, or merely passing the circle midpoint cannot finish ARC. */
  return is_center_line(mask) && now-route.exit_straight_since_ms >= SIGN_EXIT_STEADY_MS &&
      forward_mm >= SIGN_EXIT_STEADY_MM;
}
#endif

static void capture_arc(uint32_t now, SignRouteCommand *command)
{
  enter_phase(SIGN_ROUTE_ARC, now);
  route.entry_line_ready=1U;
  update_geometry();
  memset(command,0,sizeof(*command));
  command->direction=route.direction;
  command->just_finished=1U;
}

static void observe_entry_line(uint8_t mask, uint32_t now)
{
  uint8_t selected = route.direction < 0 ? 8U : 1U;
  uint8_t adjacent = route.direction < 0 ? 12U : 3U;
  int32_t minimum_yaw = route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT ?
      SIGN_GYRO_TANGENT_ENTRY_MDEG : 15000L;
  if (!route.direction || route.entry_line_ready) return;
  if (mask == selected || mask == adjacent) route.entry_edge_seen = 1U;
  /* Full middle capture is stronger than a lone inner hit on either branch.
     Small measured progress rejects a crossbar tail; 60 degrees is NOT a
     prerequisite for returning current line control to the follower. */
  if (route.entry_edge_seen && mask == 6U && route.imu_valid &&
      -route.direction * route.yaw_mdeg >= minimum_yaw)
  {
    if (!route.entry_center_active)
    { route.entry_center_active=1U; route.entry_center_ms=now; }
    else if (now-route.entry_center_ms >= SIGN_CAPTURE_MS)
      route.entry_line_ready=1U;
  }
  else route.entry_center_active=0U;
}

static void tangent_command(SignRouteCommand *command, int8_t direction)
{
  if (!command || !direction) return;
  command->active = 1U;
  command->gentle_arc = 1U;
  command->left_pwm = direction < 0 ?
      SIGN_GYRO_TANGENT_INNER_PWM : SIGN_GYRO_TANGENT_OUTER_PWM;
  command->right_pwm = direction < 0 ?
      SIGN_GYRO_TANGENT_OUTER_PWM : SIGN_GYRO_TANGENT_INNER_PWM;
}

static void straight_command(SignRouteCommand *command)
{
  command->active = 1U;
  command->gentle_arc = 0U;
  command->left_pwm = SIGN_ROUTE_PWM;
  command->right_pwm = SIGN_ROUTE_PWM;
}

static void pivot_command(SignRouteCommand *command, int8_t direction)
{
  if (!direction) return;
  command->active = 1U;
  command->gentle_arc = 0U;
  command->left_pwm = direction < 0 ? 0 : SIGN_ROUTE_PWM;
  command->right_pwm = direction < 0 ? SIGN_ROUTE_PWM : 0;
}

static void spin_command(SignRouteCommand *command, int8_t direction)
{
  if (!direction) return;
  command->active = 1U;
  command->gentle_arc = 0U;
  command->left_pwm = direction < 0 ? -SIGN_ROUTE_PWM : SIGN_ROUTE_PWM;
  command->right_pwm = (int16_t)-command->left_pwm;
}

static uint8_t narrow_line(uint8_t mask)
{
  mask &= 0x0FU;
  return mask != 0U && !is_junction(mask);
}

/* Mode 4 follows the geometry in the user's drawing as five distinct moves:
   fixed-angle entry turn, straight diagonal, sensor-followed arc, fixed-angle
   exit turn and straight diagonal. Mode 3 never enters this function. */
static uint8_t gyro_tangent_step(uint8_t line_mask, uint32_t now,
                                 SignRouteCommand *command)
{
  int32_t turn_yaw;
  int64_t heading_error;

  if (route.profile != SIGN_ROUTE_PROFILE_GYRO_TANGENT) return 0U;
  if (route.state == SIGN_ROUTE_IDLE || route.state == SIGN_ROUTE_ARMED ||
      route.state == SIGN_ROUTE_WAIT_SIGN)
    return 1U;

  if (route.state == SIGN_ROUTE_PROBE)
  {
    turn_yaw = -route.direction * route.yaw_mdeg;
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_TURN_TIMEOUT_MS ||
        turn_yaw < -15000L ||
        turn_yaw > SIGN_GYRO_TANGENT_ENTRY_MDEG + 30000L)
    {
      cancel_route(7U, now, command);
      return 1U;
    }
    if (turn_yaw >= SIGN_GYRO_TANGENT_ENTRY_MDEG)
    {
      enter_phase(SIGN_ROUTE_SELECTING, now);
      command->just_finished = 1U;
      straight_command(command);
      return 1U;
    }
    spin_command(command, route.direction);
    return 1U;
  }

  if (route.state == SIGN_ROUTE_SELECTING)
  {
    heading_error = -route.direction *
        (route.imu_yaw - route.approach_yaw) - SIGN_GYRO_TANGENT_ENTRY_MDEG;
    if (line_mask == 0U) route.departed = 1U;
    if (stable((uint8_t)(route.departed && narrow_line(line_mask) &&
                         route.travel_mm >= SIGN_GYRO_TANGENT_ENTRY_MIN_MM), now))
    {
      route.fallback_arc_hint = 0U;
      enter_phase(SIGN_ROUTE_ARC, now);
      route.entry_line_ready = 1U;
      command->active = 0U;
      command->just_finished = 1U;
      return 1U;
    }
    if (route.travel_mm >= SIGN_GYRO_TANGENT_ENTRY_SEARCH_MM)
    {
      /* The requested diagonal is only an 8-cm probe. With no arc contact,
         rotate back to the pre-stop heading instead of driving farther toward
         the sign or continuing the wrong branch. */
      enter_phase(SIGN_ROUTE_ENTRY_RETURN, now);
      command->just_started = 1U;
      spin_command(command, (int8_t)-route.direction);
      return 1U;
    }
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_ENTRY_TIMEOUT_MS ||
        route.travel_mm > SIGN_GYRO_TANGENT_ENTRY_MAX_MM ||
        heading_error < -30000L || heading_error > 30000L)
    {
      cancel_route(2U, now, command);
      return 1U;
    }
    straight_command(command);
    return 1U;
  }

  if (route.state == SIGN_ROUTE_ENTRY_RETURN)
  {
    turn_yaw = route.direction * route.yaw_mdeg;
    heading_error = route.imu_yaw - route.approach_yaw;
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_TURN_TIMEOUT_MS ||
        turn_yaw < -15000L ||
        turn_yaw > SIGN_GYRO_TANGENT_ENTRY_MDEG + 30000L)
    {
      cancel_route(8U, now, command);
      return 1U;
    }
    if (turn_yaw >= SIGN_GYRO_TANGENT_ENTRY_MDEG &&
        heading_error >= -10000LL && heading_error <= 10000LL)
    {
      enter_phase(SIGN_ROUTE_ENTRY_FALLBACK, now);
      route.departed = line_mask == 0U ? 1U : 0U;
      command->just_finished = 1U;
      straight_command(command);
      return 1U;
    }
    spin_command(command, (int8_t)-route.direction);
    return 1U;
  }

  if (route.state == SIGN_ROUTE_ENTRY_FALLBACK)
  {
    uint8_t selected_edge = route.direction < 0 ? 8U : 1U;
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_FALLBACK_TIMEOUT_MS ||
        route.travel_mm > SIGN_GYRO_TANGENT_FALLBACK_MAX_MM)
    {
      cancel_route(9U, now, command);
      return 1U;
    }
    if (line_mask == 0U) route.departed = 1U;
    straight_command(command);
    if (route.departed && line_mask != 0U)
    {
      /* First black after the confirmed white gap is the lower arc. Accept a
         broad/brief contact immediately; an ambiguous centre hit receives the
         saved direction until the selected outer sensor becomes visible. */
      route.fallback_arc_hint = 1U;
      enter_phase(SIGN_ROUTE_ARC, now);
      route.entry_line_ready = 1U;
      command->just_finished = 1U;
      if (line_mask != 15U && (line_mask & selected_edge))
      {
        route.fallback_arc_hint = 0U;
        command->active = 0U;
      }
      else tangent_command(command, route.direction);
    }
    return 1U;
  }

  if (route.state == SIGN_ROUTE_ARC)
  {
    uint8_t selected_edge = route.direction < 0 ? 8U : 1U;
    int32_t arc_yaw = route.direction * route.yaw_mdeg;
    int64_t exit_heading = route.direction *
        (route.imu_yaw - route.approach_yaw);
    if (now - route.phase_ms > SIGN_ARC_TIMEOUT_MS ||
        route.travel_mm > SIGN_ARC_MAX_MM ||
        arc_yaw < (route.fallback_arc_hint ? -SIGN_ENTRY_MAX_MDEG : -30000L) ||
        arc_yaw > SIGN_GYRO_ARC_MAX_MDEG)
    {
      cancel_route(3U, now, command);
      return 1U;
    }
    if (stable((uint8_t)(route.travel_mm >= SIGN_ARC_MIN_MM &&
                         exit_heading >= SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG), now))
    {
      enter_phase(SIGN_ROUTE_EXIT_SELECT, now);
      route.departed = 1U;
      command->just_started = 1U;
      pivot_command(command, route.direction);
      return 1U;
    }
    if (route.fallback_arc_hint)
    {
      if (line_mask != 15U && (line_mask & selected_edge))
        route.fallback_arc_hint = 0U;
      else
      {
        tangent_command(command, route.direction);
        return 1U;
      }
    }
    /* Visible circle line stays under the same live sensor follower as mode 2.
       Across a short all-white gap, keep a forward arc instead of spinning. */
    if (line_mask == 0U) tangent_command(command, (int8_t)-route.direction);
    return 1U;
  }

  if (route.state == SIGN_ROUTE_EXIT_SELECT)
  {
    turn_yaw = -route.direction * route.yaw_mdeg;
    heading_error = route.direction * (route.imu_yaw - route.approach_yaw);
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_TURN_TIMEOUT_MS ||
        turn_yaw < -15000L || turn_yaw > SIGN_SELECT_MAX_YAW_MDEG)
    {
      cancel_route(4U, now, command);
      return 1U;
    }
    if (turn_yaw >= SIGN_SELECT_CAPTURE_MIN_MDEG &&
        heading_error >= -10000LL && heading_error <= 10000LL)
    {
      enter_phase(SIGN_ROUTE_EXIT_CLEAR, now);
      command->just_finished = 1U;
      straight_command(command);
      return 1U;
    }
    pivot_command(command, route.direction);
    return 1U;
  }

  if (route.state == SIGN_ROUTE_EXIT_CLEAR)
  {
    if (line_mask == 0U) route.departed = 1U;
    if (now - route.phase_ms > SIGN_GYRO_TANGENT_EXIT_TIMEOUT_MS ||
        route.travel_mm > SIGN_GYRO_TANGENT_EXIT_MAX_MM)
    {
      cancel_route(5U, now, command);
      return 1U;
    }
    straight_command(command);
    if (stable((uint8_t)(route.departed && is_center_line(line_mask) &&
                         route.travel_mm >= SIGN_GYRO_TANGENT_EXIT_CLEAR_MM), now))
    {
      route.state = SIGN_ROUTE_LOCKED;
      route.finished_ms = now;
      route.none_since_ms = 0U;
      command->active = 0U;
      command->just_finished = 1U;
    }
    return 1U;
  }
  return 1U;
}

static void select_command(SignRouteCommand *command, uint8_t mask)
{
  uint8_t selected_edge = route.direction < 0 ? 8U : 1U;
#if SIGN_ROUTE_REQUIRE_IMU
  if (route.direction && route.imu_valid && route.state == SIGN_ROUTE_EXIT_SELECT)
  {
    int8_t steer = route.direction;
    if (route.direction * (route.imu_yaw-route.approach_yaw) < 0)
      steer = (int8_t)-steer; /* correct overshoot instead of continuing around */
    /* Only the angle-qualified EXIT phase may align without visible line.
       Entry must never cut across white while waiting for a nominal angle. */
    if (route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT)
    {
      tangent_command(command, steer);
      route.departed = 1U;
      return;
    }
    /* Continuous contact may still be the original ring. Reacquisition after
       actual white is handled before this command, without waiting for yaw
       alignment. Wide/both-side evidence cannot start a forced turn. */
    if (is_junction(mask)) return;
    steer = heading_error() > 0 ? 1 : -1;
    /* Existing KEY2 forward pivot; never request counter-rotation here. */
    command->active = 1U;
    route.departed = 1U;
    command->left_pwm = steer < 0 ? 0 : SIGN_ROUTE_PWM;
    command->right_pwm = steer < 0 ? SIGN_ROUTE_PWM : 0;
    return;
  }
#endif
  if ((route.state == SIGN_ROUTE_PROBE || route.state == SIGN_ROUTE_SELECTING) &&
      route.entry_line_ready) return;
  /* A sign is only a branch preference. It cannot drive off the black line,
     override a current centre line, or steer across an all-black bar. */
  if (route.direction == 0) return;
  if (route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT)
  {
    if (!(mask & selected_edge) && !route.departed) return;
    if (mask == 15U && !route.departed) return;
  }
  else if (!(mask & selected_edge) || mask == 15U) return;
  if (route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT)
  {
    tangent_command(command, route.direction);
    return;
  }
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
  route.last_line_mask = line_mask;
  junction = is_junction(line_mask);
  center = is_center_line(line_mask);
  if (route.step_valid && now - route.last_step_ms > SIGN_SAMPLE_MAX_GAP_MS)
  {
    route.exit_straight_active = 0U;
    route.capture_active = 0U;
    route.entry_center_active = 0U;
    route.junction_active = 0U;
  }
  route.step_valid = 1U;
  route.last_step_ms = now;
  update_geometry();
#if SIGN_ROUTE_REQUIRE_IMU
  if (!route.imu_valid && route.state != SIGN_ROUTE_IDLE &&
      route.state != SIGN_ROUTE_LOCKED && route.state != SIGN_ROUTE_CANCELLED)
  {
    cancel_route(6U, now, command);
    return;
  }
#endif

  if (route.observation_pause_active)
  {
    /* Camera voting continues in ObserveDetection, but stationary sensor
       samples cannot enter PROBE/ARC or prepare a hidden motor command. */
    route.junction_active=route.capture_active=route.entry_center_active=0U;
    route.exit_straight_active=0U;
    return;
  }

  /* All-white belongs to SL2's continuous counter-rotation search. Neither
     elapsed time nor a fresh sign is allowed to replace it with a zero target. */
  if (route.state == SIGN_ROUTE_ARMED &&
      route.direction != 0 &&
      (now - route.armed_ms > SIGN_PENDING_MAX_AGE_MS ||
       now - route.last_frame_ms > SIGN_ONLINE_MAX_AGE_MS))
  {
    route.direction = 0;
    route.approach_from_pause=0U;
    clear_window();
    if (route.state == SIGN_ROUTE_ARMED) route.state = SIGN_ROUTE_IDLE;
  }
  if (route.state == SIGN_ROUTE_LOCKED || route.state == SIGN_ROUTE_CANCELLED)
  {
    if (now - route.finished_ms >= SIGN_REARM_COOLDOWN_MS &&
        route.none_since_ms != 0U &&
        now - route.none_since_ms >= SIGN_REARM_NONE_MS &&
        now - route.last_frame_ms <= SIGN_ONLINE_MAX_AGE_MS &&
        route.last_class == -1)
    {
      route.state = SIGN_ROUTE_IDLE;
      route.direction = 0;
      route.fault = 0U;
      route.junction_active = 0U;
      clear_window();
    }
    return;
  }

  if (gyro_tangent_step(line_mask, now, command))
  {
    command->direction = route.direction;
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
#if SIGN_ROUTE_REQUIRE_IMU
    if (!route.imu_valid) { cancel_route(6U, now, command); return; }
#endif
    enter_phase(SIGN_ROUTE_PROBE, now);
    command->just_started = 1U;
  }

  if (route.state == SIGN_ROUTE_PROBE)
  {
    observe_entry_line(line_mask, now);
#if SIGN_PROBE_HOLD_MS > 0
    if (route.direction != 0)
    {
      if (!route.probe_hold_started)
      {
        route.probe_hold_started = 1U;
        route.probe_hold_since_ms = now;
      }
      if (now - route.probe_hold_since_ms < SIGN_PROBE_HOLD_MS)
      {
        uint8_t selected_edge = route.direction < 0 ? 8U : 1U;
        /* Remember the choice, not a compulsory ten-second motor turn.
           A crossbar alone does not prove that we took a branch. */
        if (line_mask != 15U && (line_mask & selected_edge))
          route.departed = 1U;
        uint8_t angle_ready = 1U;
#if SIGN_ROUTE_REQUIRE_IMU
        int32_t entry_angle = -route.direction * route.yaw_mdeg;
        if (entry_angle > SIGN_ENTRY_MAX_MDEG || entry_angle < -30000L)
        { cancel_route(7U, now, command); return; }
        angle_ready = entry_angle >= SIGN_ENTRY_MIN_MDEG;
        if (angle_ready) route.departed = 1U;
#endif
        if (route.entry_line_ready)
        {
          capture_arc(now, command);
          return;
        }
        if (stable(route.departed && center && angle_ready, now))
        {
          enter_phase(SIGN_ROUTE_ARC, now);
          command->just_finished = 1U;
          return;
        }
        /* Remember the branch, but grant motor ownership only while its
           selected edge is actually visible. Center/white releases it. */
        select_command(command, line_mask);
        return;
      }
      /* Ten seconds is a maximum selection window, never a required turn. */
      cancel_route(1U, now, command);
      return;
    }
#endif
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
    /* Split arcs can keep both outside sensors black while the middle is
       white. This is already branch evidence; waiting for !junction lets
       ordinary tracking choose a side first. All-black remains a crossbar. */
    uint8_t split = line_mask == 9U;
    if (stable((uint8_t)(split ? 3U : (!junction ? (center ? 1U : 2U) : 0U)), now) &&
        (!center || now - route.capture_since_ms >= SIGN_PROBE_CENTER_CLEAR_MS))
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
    uint32_t timeout = route.state == SIGN_ROUTE_EXIT_SELECT ?
        SIGN_EXIT_ALIGN_TIMEOUT_MS : SIGN_SELECT_TIMEOUT_MS;
#if SIGN_ROUTE_REQUIRE_IMU
    if (route.profile == SIGN_ROUTE_PROFILE_STANDARD &&
        route.state == SIGN_ROUTE_EXIT_SELECT)
    {
      if (line_mask == 0U) route.exit_line_lost=1U;
      else if (route.exit_line_lost)
      {
        /* The first new black contact ends forced exit turning immediately,
           even before the estimated heading aligns. A broad contact releases
           motors too; only later stable middle contact may complete the route. */
        enter_phase(SIGN_ROUTE_EXIT_CLEAR,now);
        command->just_finished=1U;
        return; /* command was cleared at Step entry; no stale pivot survives */
      }
    }
#endif
    if (route.state == SIGN_ROUTE_SELECTING) observe_entry_line(line_mask, now);
    select_command(command, line_mask);
    if (now - route.phase_ms > timeout ||
        (route.state == SIGN_ROUTE_EXIT_SELECT && route.profile == SIGN_ROUTE_PROFILE_STANDARD ?
          (route.yaw_mdeg > SIGN_SELECT_MAX_YAW_MDEG || route.yaw_mdeg < -SIGN_SELECT_MAX_YAW_MDEG) :
          (turn_yaw > SIGN_SELECT_MAX_YAW_MDEG || turn_yaw < -30000L)))
    {
      cancel_route(2U, now, command);
      return;
    }
    if (route.state == SIGN_ROUTE_SELECTING && route.entry_line_ready)
    {
      capture_arc(now, command);
      return;
    }
#if SIGN_ROUTE_REQUIRE_IMU
    if (route.state == SIGN_ROUTE_EXIT_SELECT && route.profile == SIGN_ROUTE_PROFILE_STANDARD)
    {
      int64_t error = route.direction * heading_error();
      int32_t magnitude = (int32_t)(error < 0 ? -error : error);
      uint8_t aligned = (error >= -SIGN_EXIT_ALIGN_MDEG && error <= SIGN_EXIT_ALIGN_MDEG) ||
          (((route.exit_previous_error > 0 && error <= 0) ||
            (route.exit_previous_error < 0 && error >= 0)) &&
           error >= -SIGN_EXIT_CAPTURE_MDEG && error <= SIGN_EXIT_CAPTURE_MDEG);
      route.exit_previous_error=error;
      if (magnitude < route.exit_best_error_mdeg) route.exit_best_error_mdeg=magnitude;
      if (command->active && magnitude > route.exit_best_error_mdeg+SIGN_EXIT_DIVERGE_MDEG)
      { cancel_route(2U, now, command); return; }
      if (aligned)
      {
        enter_phase(SIGN_ROUTE_EXIT_CLEAR, now);
        command->gentle_arc=0U;
        /* Heading alignment ends route motor ownership immediately. Current
           line correction or actual line-loss search now belongs to tracking;
           there is no separate mode-3 blind straight segment to latch. */
        command->active=0U;
        command->left_pwm=command->right_pwm=0;
        return;
      }
      return; /* Entry's minimum-turn/capture rules do not apply to exit alignment. */
    }
    /* Mode 4 keeps its separately commissioned gyro-tangent trajectory. */
    if (route.state == SIGN_ROUTE_EXIT_SELECT)
    {
      int64_t error = route.direction * (route.imu_yaw-route.approach_yaw);
      uint8_t aligned = (error >= -10000 && error <= 10000) ||
          (((route.exit_previous_error > 0 && error <= 0) ||
            (route.exit_previous_error < 0 && error >= 0)) &&
           error >= -15000 && error <= 15000);
      route.exit_previous_error=error;
      if (turn_yaw >= SIGN_SELECT_CAPTURE_MIN_MDEG && aligned)
      {
        enter_phase(SIGN_ROUTE_EXIT_CLEAR, now);
        command->active=1U;
        command->gentle_arc=0U;
        command->left_pwm=command->right_pwm=SIGN_ROUTE_PWM;
        return;
      }
    }
#endif
    if (!junction && !center) route.departed = 1U;
    if (stable(route.departed && center
#if SIGN_ROUTE_REQUIRE_IMU
        && turn_yaw >= SIGN_SELECT_CAPTURE_MIN_MDEG
        && (route.state != SIGN_ROUTE_EXIT_SELECT ||
            (route.imu_yaw - route.approach_yaw >= -15000 &&
             route.imu_yaw - route.approach_yaw <= 15000))
#endif
        , now))
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
#if !SIGN_ROUTE_REQUIRE_IMU
    uint8_t exit_side = route.direction < 0 ? 8U : 1U;
#endif
    directed_arc_yaw = route.direction * route.yaw_mdeg;
#if SIGN_ROUTE_REQUIRE_IMU
    int32_t road_heading=route.direction * heading_error();
    /* Mode 4 has already dispatched above. Mode 3 locates the exit region
       directly from the signed straight-road reference, not an estimated
       entry tangent or an accumulated 150/170-degree arc. */
    if (road_heading < -SIGN_ENTRY_MAX_MDEG || road_heading > SIGN_EXIT_HEADING_MAX_MDEG)
    {
      cancel_route(3U, now, command);
      return;
    }
#endif
    if (now - route.phase_ms > SIGN_ARC_TIMEOUT_MS || route.travel_mm > SIGN_ARC_MAX_MM
#if !SIGN_ROUTE_REQUIRE_IMU
        || directed_arc_yaw > SIGN_ARC_MAX_YAW_MDEG || directed_arc_yaw < -90000L
#endif
        )
    {
      cancel_route(3U, now, command);
      return;
    }
#if SIGN_ROUTE_REQUIRE_IMU
    if (route.profile == SIGN_ROUTE_PROFILE_STANDARD)
    {
      int32_t error=heading_error();
      uint8_t exit_edge=route.direction < 0 ? 8U : 1U;
      if (is_track_line(line_mask) && route.arc_origin_locked && route.travel_mm >= SIGN_ARC_MIN_MM &&
          directed_arc_yaw > route.arc_peak_mdeg)
        route.arc_peak_mdeg=directed_arc_yaw; /* diagnostic only; not an exit threshold */
      if (is_track_line(line_mask) && road_heading <= -15000L) route.arc_lower_seen=1U;
      if (route.arc_lower_seen && is_track_line(line_mask) && route.travel_mm >= SIGN_ARC_MIN_MM &&
          road_heading > route.exit_heading_peak_mdeg)
        route.exit_heading_peak_mdeg=road_heading;
      if (natural_departure_confirmed(line_mask,road_heading,now))
      {
        complete_route(now,command);
        return;
      }
      if (is_track_line(line_mask) && route.travel_mm >= SIGN_ARC_MIN_MM &&
          road_heading >= SIGN_EXIT_HEADING_MIN_MDEG)
        route.exit_region_seen=1U;
      uint8_t natural_exit=route.exit_region_seen && center &&
          error >= -SIGN_EXIT_CAPTURE_MDEG && error <= SIGN_EXIT_CAPTURE_MDEG;
      uint8_t edge_ready=(line_mask & exit_edge) &&
          (road_heading >= SIGN_EXIT_EDGE_HEADING_MDEG ||
           (route.arc_lower_seen && road_heading >= SIGN_EXIT_EARLY_MIN_MDEG &&
            route.exit_heading_peak_mdeg-road_heading >= SIGN_EXIT_EARLY_DROP_MDEG));
      uint8_t turn_ready=is_track_line(line_mask) &&
          route.travel_mm >= SIGN_ARC_MIN_MM &&
          (edge_ready || road_heading >= SIGN_EXIT_HEADING_TRIGGER_MDEG);
      /* All-white search and full-black backgrounds cannot trigger exit.
         The signed upper-half heading, not a guessed apex, authorizes it. */
      if (stable(natural_exit ? 2U : (turn_ready ? 1U : 0U), now))
      {
        if (natural_exit)
        {
          complete_route(now,command);
          return; /* Already on the aligned outgoing line: no extra route phase. */
        }
        enter_phase(SIGN_ROUTE_EXIT_SELECT, now);
        route.departed=1U;
        command->just_started=1U;
        select_command(command,line_mask);
      }
      return;
    }
#endif
    if (stable(route.travel_mm >= SIGN_ARC_MIN_MM && directed_arc_yaw >=
#if SIGN_ROUTE_REQUIRE_IMU
               SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG, now))
#else
               SIGN_ARC_MIN_YAW_MDEG &&
               line_mask == (exit_side == 8U ? 12U : 3U), now))
#endif
    {
      enter_phase(SIGN_ROUTE_EXIT_SELECT, now);
      route.departed = 1U;
      command->just_started = 1U;
      select_command(command, line_mask);
    }
    else if (route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT)
    {
      /* The circle bends opposite the entry/exit transition. Keeping both
         wheels forward prevents the old in-place U-turn and makes yaw the
         phase boundary instead of a particular line-mask coincidence. */
      tangent_command(command, (int8_t)-route.direction);
    }
    return;
  }

  if (route.state == SIGN_ROUTE_EXIT_CLEAR)
  {
    uint32_t exit_timeout = route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT ?
        SIGN_GYRO_TANGENT_EXIT_TIMEOUT_MS : SIGN_EXIT_CLEAR_TIMEOUT_MS;
    int32_t exit_max_mm = route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT ?
        SIGN_GYRO_TANGENT_EXIT_MAX_MM : SIGN_EXIT_STRAIGHT_MAX_MM;
    int32_t exit_min_mm = route.profile == SIGN_ROUTE_PROFILE_GYRO_TANGENT ?
        SIGN_GYRO_TANGENT_EXIT_CLEAR_MM : SIGN_EXIT_CLEAR_MM;
    if (now - route.phase_ms > exit_timeout || route.travel_mm > exit_max_mm)
    {
      cancel_route(5U, now, command);
      return;
    }
#if SIGN_ROUTE_REQUIRE_IMU
    if (route.profile == SIGN_ROUTE_PROFILE_STANDARD)
    {
      exit_min_mm=0L;
      /* Completion confirmation is passive: never take the motors back,
         including on white, a broad mark, or a failed line capture. */
    }
    else
    {
      command->active=1U;
      command->left_pwm=command->right_pwm=SIGN_ROUTE_PWM;
    }
#endif
    if (stable(center && route.travel_mm >= exit_min_mm, now))
    {
      if (route.profile == SIGN_ROUTE_PROFILE_STANDARD) complete_route(now,command);
      else
      {
        route.state = SIGN_ROUTE_LOCKED;
        route.finished_ms = now;
        route.none_since_ms = 0U;
        command->active = 0U;
        command->just_finished = 1U;
      }
    }
  }
  command->direction = route.direction;
}

void SignRoute_GetStatus(uint32_t now, SignRouteStatus *status)
{
  if (status == NULL) return;
  status->state = route.state;
  status->yaw_valid = route.imu_valid;
  status->entry_line_ready = route.entry_line_ready;
  status->direction = route.direction;
  status->last_class = route.last_class;
  status->last_score = route.last_score;
  status->vision_online = route.frame_valid &&
      now - route.last_frame_ms <= SIGN_ONLINE_MAX_AGE_MS ? 1U : 0U;
  status->last_sequence = route.last_sequence;
  status->searching = route.step_valid && route.last_line_mask == 0U;
  status->fault = route.fault;
  status->travel_mm = route.travel_mm;
  status->yaw_mdeg = route.yaw_mdeg;
  status->profile = route.profile;
  status->heading_error_mdeg = heading_error();
  status->arc_peak_mdeg = route.arc_peak_mdeg;
  status->approach_from_pause = route.approach_from_pause;
  status->exit_heading_peak_mdeg = route.exit_heading_peak_mdeg;
}
