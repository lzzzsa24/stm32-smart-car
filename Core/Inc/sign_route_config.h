#ifndef SIGN_ROUTE_CONFIG_H
#define SIGN_ROUTE_CONFIG_H
/* Geometry/time bounds are navigation warnings; all-white keeps searching. */
#define SIGN_PROBE_MAX_MM 80L
#define SIGN_PROBE_TIMEOUT_MS 1800U
#ifndef SIGN_PROBE_HOLD_MS
#define SIGN_PROBE_HOLD_MS 10000U
#endif
#define SIGN_CAPTURE_MS 30U
#ifndef SIGN_ROUTE_REQUIRE_IMU
#define SIGN_ROUTE_REQUIRE_IMU 1
#endif
#define SIGN_ENTRY_MIN_MDEG 60000L
#define SIGN_ENTRY_MAX_MDEG 120000L
#define SIGN_GYRO_ARC_MAX_MDEG 225000L
#define SIGN_SELECT_CAPTURE_MIN_MDEG 45000L
#define SIGN_EXIT_ALIGN_MDEG 25000L
#define SIGN_EXIT_CAPTURE_MDEG 30000L
#define SIGN_EXIT_DIVERGE_MDEG 15000L
/* Mode 3: signed yaw relative to the completed two-second observation stop.
   Multiply MPU yaw error by sign direction: lower half <0, upper half >0. */
/* Mode3 starts exit from signed heading alone at40 degrees, independently of
   line mask, encoder travel and debounce. Alignment only releases steering;
   the selected outer clear-to-black event still completes the route. */
#define SIGN_EXIT_HEADING_TRIGGER_MDEG 40000L
#define SIGN_MODE3_EXIT_MIN_DEG 30U
#define SIGN_MODE3_EXIT_MAX_DEG 90U
#define SIGN_MODE3_EXIT_STEP_DEG 5U
#define SIGN_EXIT_HEADING_MIN_MDEG 45000L
#define SIGN_EXIT_HEADING_MAX_MDEG 120000L
#define SIGN_ARC_ORIGIN_LOCK_MDEG 20000L
#define SIGN_EXIT_ALIGN_TIMEOUT_MS 6000U
#define SIGN_EXIT_STRAIGHT_MAX_MM 250L
#define SIGN_PROBE_CENTER_CLEAR_MS 120U
#define SIGN_SAMPLE_MAX_GAP_MS 50U
#define SIGN_SELECT_TIMEOUT_MS 2500U
#define SIGN_SELECT_MAX_YAW_MDEG 135000L
#define SIGN_ARC_MIN_MM 150L
#define SIGN_ARC_MAX_MM 3000L
#define SIGN_ARC_MIN_YAW_MDEG 90000L
#define SIGN_ARC_MAX_YAW_MDEG 270000L
#define SIGN_ARC_TIMEOUT_MS 20000U
#define SIGN_EXIT_CLEAR_MM 60L
#define SIGN_EXIT_CLEAR_TIMEOUT_MS 2000U
#define SIGN_ROUTE_PWM 2200
/* Mode-4 trajectory from the supplied drawing: turn to a diagonal heading,
   drive straight to the selected arc, follow that arc with live sensors, turn
   back to the approach heading, then drive straight to the outgoing line. */
#define SIGN_GYRO_TANGENT_ENTRY_MDEG 45000L
#define SIGN_GYRO_TANGENT_EXIT_HEADING_MDEG 60000L
#define SIGN_GYRO_TANGENT_INNER_PWM 2200
#define SIGN_GYRO_TANGENT_OUTER_PWM 2400
#define SIGN_GYRO_TANGENT_ENTRY_MIN_MM 40L
#define SIGN_GYRO_TANGENT_ENTRY_SEARCH_MM 80L
#define SIGN_GYRO_TANGENT_ENTRY_MAX_MM 400L
#define SIGN_GYRO_TANGENT_ENTRY_TIMEOUT_MS 3000U
#define SIGN_GYRO_TANGENT_TURN_TIMEOUT_MS 3000U
#define SIGN_GYRO_TANGENT_FALLBACK_MAX_MM 450L
#define SIGN_GYRO_TANGENT_FALLBACK_TIMEOUT_MS 3500U
#define SIGN_GYRO_TANGENT_EXIT_CLEAR_MM 80L
#define SIGN_GYRO_TANGENT_EXIT_MAX_MM 450L
#define SIGN_GYRO_TANGENT_EXIT_TIMEOUT_MS 3500U
#endif
