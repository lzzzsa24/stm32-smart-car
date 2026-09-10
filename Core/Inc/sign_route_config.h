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
#define SIGN_EXIT_ALIGN_MDEG 10000L
#define SIGN_EXIT_CAPTURE_MDEG 15000L
#define SIGN_EXIT_DIVERGE_MDEG 15000L
/* Mode 3: signed yaw relative to a moving, centered-line road estimate.
   Multiply MPU yaw error by sign direction: lower half <0, upper half >0. */
#define SIGN_ROAD_SAMPLE_MS 200U
#define SIGN_ROAD_SAMPLE_MM 60L
#define SIGN_ROAD_SAMPLE_RANGE_MDEG 4000L
#define SIGN_ROAD_REFERENCE_MAX_AGE_MS 5000U
/* Offset-independent departure: sustained arc progress followed by a return.
   Once observed, old ARC motor authority can never be reacquired. */
#define SIGN_DEPART_ARC_PROGRESS_MDEG 110000L
#define SIGN_DEPART_MAX_PROGRESS_MDEG 200000L
#define SIGN_DEPART_RETURN_MDEG 15000L
#define SIGN_DEPART_VERIFY_TIMEOUT_MS 600U
#define SIGN_DEPART_VERIFY_MAX_MM 140L
/* Leave turning room before the 90-degree tangent at the far junction.
   Keep natural-exit evidence separate: an earlier turn request is not proof
   that the car has already joined the outgoing straight. */
#define SIGN_EXIT_EDGE_HEADING_MDEG 55000L
#define SIGN_EXIT_HEADING_TRIGGER_MDEG 65000L
#define SIGN_EXIT_HEADING_MIN_MDEG 70000L
#define SIGN_EXIT_HEADING_MAX_MDEG 120000L
/* A real outward correction can start before the nominal 55/65-degree gates. */
#define SIGN_EXIT_EARLY_MIN_MDEG 35000L
#define SIGN_EXIT_EARLY_DROP_MDEG 8000L
/* Natural departure: upper-half evidence, heading return and forward line travel. */
#define SIGN_EXIT_RETURN_PEAK_MDEG 25000L
#define SIGN_EXIT_RETURN_DROP_MDEG 15000L
#define SIGN_EXIT_RETURN_RANGE_MDEG 30000L
#define SIGN_EXIT_STEADY_RANGE_MDEG 6000L
#define SIGN_EXIT_STEADY_MS 120U
#define SIGN_EXIT_STEADY_MM 40L
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
