#include "sign_route.h"

#include <stddef.h>
#include <string.h>

#define SIGN_WINDOW_SIZE                 5U
#define SIGN_SCORE_MINIMUM              20U
#define SIGN_WINDOW_MAX_SPAN_MS        600U
#define SIGN_ONLINE_MAX_AGE_MS         500U
#define SIGN_PENDING_MAX_AGE_MS       5000U
#define SIGN_CENTER_MAX_JUMP_PX         60U
#define SIGN_JUNCTION_CONFIRM_MS        20U
#define SIGN_SELECT_MIN_MS             120U
#define SIGN_CAPTURE_CONFIRM_MS         20U
#define SIGN_REARM_COOLDOWN_MS        1500U
#define SIGN_REARM_NONE_MS             800U
#define SIGN_SELECT_TURN_PWM           2700

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
  uint32_t junction_since_ms;
  uint32_t select_started_ms;
  uint32_t capture_since_ms;
  uint32_t finished_ms;
  uint32_t none_since_ms;
  uint8_t junction_active;
  uint8_t departed;
  uint8_t capture_active;
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

  if (route.state != SIGN_ROUTE_IDLE || route.count < 3U)
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
    route.state = SIGN_ROUTE_ARMED;
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

  if (detection == NULL || detection->sequence == route.last_sequence)
  {
    return;
  }
  if (route.last_sequence != 0U &&
      detection->sequence != route.last_sequence + 1U)
  {
    clear_window();
  }
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

void SignRoute_Step(uint8_t line_mask,
                    uint32_t now,
                    SignRouteCommand *command)
{
  uint8_t junction;

  if (command == NULL)
  {
    return;
  }
  memset(command, 0, sizeof(*command));
  command->direction = route.direction;

  if (route.state == SIGN_ROUTE_ARMED &&
      now - route.armed_ms > SIGN_PENDING_MAX_AGE_MS)
  {
    route.state = SIGN_ROUTE_IDLE;
    route.direction = 0;
    route.junction_active = 0U;
    clear_window();
  }
  if (route.state == SIGN_ROUTE_LOCKED &&
      now - route.finished_ms >= SIGN_REARM_COOLDOWN_MS &&
      route.none_since_ms != 0U &&
      now - route.none_since_ms >= SIGN_REARM_NONE_MS)
  {
    route.state = SIGN_ROUTE_IDLE;
    route.direction = 0;
    clear_window();
  }

  junction = is_junction(line_mask);
  if (route.state == SIGN_ROUTE_ARMED)
  {
    if (junction != 0U)
    {
      if (route.junction_active == 0U)
      {
        route.junction_active = 1U;
        route.junction_since_ms = now;
      }
      if (now - route.junction_since_ms >= SIGN_JUNCTION_CONFIRM_MS)
      {
        route.state = SIGN_ROUTE_SELECTING;
        route.select_started_ms = now;
        route.departed = 0U;
        route.capture_active = 0U;
        command->just_started = 1U;
      }
    }
    else
    {
      route.junction_active = 0U;
    }
  }

  if (route.state != SIGN_ROUTE_SELECTING)
  {
    command->direction = route.direction;
    return;
  }

  command->active = 1U;
  command->direction = route.direction;
  command->left_pwm = route.direction < 0 ? -SIGN_SELECT_TURN_PWM :
                                            SIGN_SELECT_TURN_PWM;
  command->right_pwm = (int16_t)-command->left_pwm;

  if (now - route.select_started_ms >= 80U && junction == 0U)
  {
    route.departed = 1U;
  }
  if (route.departed != 0U &&
      now - route.select_started_ms >= SIGN_SELECT_MIN_MS &&
      is_center_line(line_mask) != 0U)
  {
    if (route.capture_active == 0U)
    {
      route.capture_active = 1U;
      route.capture_since_ms = now;
    }
    if (now - route.capture_since_ms >= SIGN_CAPTURE_CONFIRM_MS)
    {
      route.state = SIGN_ROUTE_LOCKED;
      route.finished_ms = now;
      route.capture_active = 0U;
      command->active = 0U;
      command->just_finished = 1U;
    }
  }
  else
  {
    route.capture_active = 0U;
  }
}

void SignRoute_GetStatus(uint32_t now, SignRouteStatus *status)
{
  if (status == NULL)
  {
    return;
  }
  status->state = route.state;
  status->direction = route.direction;
  status->last_class = route.last_class;
  status->last_score = route.last_score;
  status->vision_online = route.last_sequence != 0U &&
      now - route.last_frame_ms <= SIGN_ONLINE_MAX_AGE_MS ? 1U : 0U;
  status->last_sequence = route.last_sequence;
}
