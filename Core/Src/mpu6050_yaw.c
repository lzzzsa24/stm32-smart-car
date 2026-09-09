#include "mpu6050_yaw.h"
#include <string.h>

/* 100 Hz FIFO, accel +/-2 g, gyro +/-500 deg/s (65.5 LSB/deg/s).
   FIFO order: AX AY AZ GX GY GZ, big endian, no temperature. */
#define FRAME_BYTES 12U
#define MAX_FRAMES 8U
#define CAL_SAMPLES 200U
/* Candidate bias envelope at 65.5 LSB/(deg/s). Uncorrected DC offset must
   not be treated as motion. Stability still requires <=65 raw span on ALL
   axes for 200 consecutive samples and a stopped, upright platform. */
#define CAL_RAW_LIMIT 1310
/* Yaw uses gyro Z, not acceleration magnitude. Allow a bounded stable gravity
   reference (0.75..1.5 nominal g), but do not label a biased accelerometer
   calibrated. Reject changing acceleration throughout the same 2 s window. */
#define CAL_Z_MIN 12288
#define CAL_Z_MAX 24576
#define CAL_ACCEL_SPAN 800
static MpuYawReading reading;
static uint32_t stage_ms, last_poll_ms, calibration_ms;
static uint8_t init_stage;
static int64_t bias_sum, yaw_udeg;
static int16_t cal_min[3], cal_max[3];
static int16_t accel_min[3], accel_max[3];
static int32_t accel_sum[3];
static uint8_t bias_valid, restore_bias, ready_sample;
static uint32_t service_ms, fault_ms;

static int32_t magnitude(int32_t value) { return value < 0 ? -value : value; }
static int16_t signed_be(const uint8_t *p)
{
  uint16_t v = (uint16_t)((uint16_t)p[0] * 256U + p[1]);
  return (int16_t)(v < 32768U ? (int32_t)v : (int32_t)v - 65536);
}
static void fault(uint8_t reason)
{
  reading.state = MPU_YAW_FAULT;
  reading.fault = reason;
  reading.last_fault = reason;
  fault_ms = service_ms;
  reading.rate_mdeg_s = 0;
}
static void clear_calibration(void)
{
  reading.calibration_samples = 0;
  bias_sum = 0;
  memset(accel_sum, 0, sizeof(accel_sum));
}
void MpuYaw_Init(uint32_t now_ms)
{
  memset(&reading, 0, sizeof(reading));
  bias_valid = restore_bias = ready_sample = 0U;
  service_ms = now_ms;
  yaw_udeg = bias_sum = 0;
  clear_calibration();
  stage_ms = last_poll_ms = calibration_ms = now_ms;
  init_stage = 0;
  if (!MpuBus_Init() || !MpuBus_Write(0x6bU, 0x80U)) fault(MPU_FAULT_BUS);
}

static void sample(const uint8_t *p, uint8_t stationary)
{
  int32_t ax = signed_be(p), ay = signed_be(p + 2), az = signed_be(p + 4);
  int16_t gyro[3];
  unsigned i;
  for (i = 0; i < 3; ++i) gyro[i] = signed_be(p + 6 + i * 2);
  reading.raw_accel[0] = (int16_t)ax;
  reading.raw_accel[1] = (int16_t)ay;
  reading.raw_accel[2] = (int16_t)az;
  for (i = 0; i < 3; ++i) reading.raw_gyro[i] = gyro[i];
  ++reading.samples;
  if (reading.state == MPU_YAW_WAIT_STATIONARY) return;
  if (reading.state == MPU_YAW_CALIBRATING)
  {
    /* Reject a tilted/upside-down mounting and visible motion. Constant very
       slow hand rotation is not distinguishable from bias: keep car still. */
    uint8_t reject = 0U;
    if (!stationary) reject |= MPU_CAL_NOT_STOPPED;
    if (magnitude(ax) > 4500 || magnitude(ay) > 4500) reject |= MPU_CAL_TILT;
    if (az * MPU6050_YAW_SIGN < CAL_Z_MIN || az * MPU6050_YAW_SIGN > CAL_Z_MAX)
      reject |= MPU_CAL_Z;
    if (magnitude(gyro[0]) > CAL_RAW_LIMIT || magnitude(gyro[1]) > CAL_RAW_LIMIT ||
        magnitude(gyro[2]) > CAL_RAW_LIMIT) reject |= MPU_CAL_RAW_LIMIT;
    reading.cal_reject = reject;
    if (reject)
    {
      reading.cal_last_reject = reject; ++reading.cal_rejections;
      clear_calibration(); return;
    }
    for (i = 0; i < 3; ++i)
    {
      int16_t a = reading.raw_accel[i];
      if (!reading.calibration_samples) accel_min[i] = accel_max[i] = a;
      if (a < accel_min[i]) accel_min[i] = a;
      if (a > accel_max[i]) accel_max[i] = a;
      if ((int32_t)accel_max[i] - accel_min[i] > CAL_ACCEL_SPAN)
      {
        reading.cal_reject = reading.cal_last_reject = MPU_CAL_ACCEL_UNSTABLE;
        ++reading.cal_rejections; clear_calibration(); return;
      }
      if (!reading.calibration_samples) cal_min[i] = cal_max[i] = gyro[i];
      if (gyro[i] < cal_min[i]) cal_min[i] = gyro[i];
      if (gyro[i] > cal_max[i]) cal_max[i] = gyro[i];
      if (cal_max[i] - cal_min[i] > 65)
      {
        reading.cal_reject = reading.cal_last_reject = MPU_CAL_UNSTABLE;
        ++reading.cal_rejections; clear_calibration(); return;
      }
    }
    bias_sum += gyro[2];
    for (i = 0; i < 3; ++i) accel_sum[i] += reading.raw_accel[i];
    if (++reading.calibration_samples == CAL_SAMPLES)
    {
      reading.bias_milliraw = (int32_t)(bias_sum * 1000 / CAL_SAMPLES);
      for (i = 0; i < 3; ++i)
        reading.cal_accel_mean[i] = (int16_t)(accel_sum[i] / (int32_t)CAL_SAMPLES);
      reading.accel_reference_warning =
          reading.cal_accel_mean[2] * MPU6050_YAW_SIGN < 14000 ||
          reading.cal_accel_mean[2] * MPU6050_YAW_SIGN > 18500;
      reading.state = MPU_YAW_READY;
      bias_valid = ready_sample = 1U;
    }
    return;
  }
  if (magnitude(gyro[0]) >= 32000 || magnitude(gyro[1]) >= 32000 ||
      magnitude(gyro[2]) >= 32000)
  { fault(MPU_FAULT_RANGE); return; }
  {
    int32_t corrected = (gyro[2] * 1000 - reading.bias_milliraw) * MPU6050_YAW_SIGN;
    ready_sample = 1U;
    reading.rate_mdeg_s = (int32_t)((int64_t)corrected * 2 / 131);
    /* Integrate each sensor-timed FIFO sample, never a delayed main-loop dt. */
    yaw_udeg += (int64_t)corrected * 20 / 131;
    reading.yaw_mdeg = yaw_udeg / 1000;
  }
}

void MpuYaw_Task(uint32_t now_ms, uint8_t stationary)
{
  uint8_t bytes[FRAME_BYTES * MAX_FRAMES], count_bytes[2], status;
  uint16_t count, frames, i;
  service_ms = now_ms;
  if (reading.state == MPU_YAW_FAULT)
  {
    /* Retry the peripheral cooperatively, at most once per second. Retain a
       completed bias, but announce the lost yaw interval to every consumer. */
    if (now_ms - fault_ms >= 1000U)
    {
      MpuYawReading previous = reading;
      uint8_t saved_bias_valid = bias_valid;
      int64_t previous_udeg = yaw_udeg;
      MpuYaw_Init(now_ms);
      bias_valid = restore_bias = saved_bias_valid;
      reading.bias_milliraw = previous.bias_milliraw;
      reading.calibration_samples = saved_bias_valid ? CAL_SAMPLES : 0U;
      reading.yaw_mdeg = previous.yaw_mdeg; yaw_udeg = previous_udeg;
      reading.generation = previous.generation + 1U;
      reading.restart_count = previous.restart_count + 1U;
      reading.last_fault = previous.fault;
      reading.samples = previous.samples;
      reading.cal_last_reject = previous.cal_last_reject;
      reading.cal_rejections = previous.cal_rejections;
      memcpy(reading.cal_accel_mean, previous.cal_accel_mean, sizeof(reading.cal_accel_mean));
      reading.accel_reference_warning = previous.accel_reference_warning;
      reading.peak_fifo_bytes = previous.peak_fifo_bytes;
      reading.max_service_gap_ms = previous.max_service_gap_ms;
      reading.backlog_events = previous.backlog_events;
    }
    return;
  }
  if (reading.state == MPU_YAW_STARTING)
  {
    if (now_ms - stage_ms < 100U) return;
    stage_ms = now_ms;
    if (init_stage == 0U)
    {
      if (!MpuBus_Read(0x75U, &status, 1U)) { fault(MPU_FAULT_BUS); return; }
      if (status != 0x68U) { fault(MPU_FAULT_ID); return; }
      if (!MpuBus_Write(0x6bU, 1U)) { fault(MPU_FAULT_BUS); return; }
      init_stage = 1U;
      return;
    }
    /* Enable FIFO only after all sources/clock/filter settings are written. */
    if (!MpuBus_Write(0x6cU, 0U) || !MpuBus_Write(0x23U, 0U) ||
        !MpuBus_Write(0x19U, 9U) || !MpuBus_Write(0x1aU, 3U) ||
        !MpuBus_Write(0x1bU, 8U) || !MpuBus_Write(0x1cU, 0U) ||
        !MpuBus_Write(0x38U, 0U) || !MpuBus_Write(0x6aU, 4U) ||
        !MpuBus_Write(0x6aU, 0x40U) || !MpuBus_Write(0x23U, 0x78U))
    { fault(MPU_FAULT_BUS); return; }
    {
      static const uint8_t registers[] = {0x6b, 0x6c, 0x19, 0x1a, 0x1b, 0x1c, 0x6a, 0x23};
      static const uint8_t expected[] = {1, 0, 9, 3, 8, 0, 0x40, 0x78};
      unsigned reg_index;
      for (reg_index = 0; reg_index < sizeof(registers); ++reg_index)
        if (!MpuBus_Read(registers[reg_index], &status, 1U) || status != expected[reg_index])
        { fault(MPU_FAULT_BUS); return; }
    }
    reading.state = restore_bias ? MPU_YAW_READY :
        (stationary ? MPU_YAW_CALIBRATING : MPU_YAW_WAIT_STATIONARY);
    reading.last_sample_ms = last_poll_ms = calibration_ms = now_ms;
    return;
  }
  if (reading.state == MPU_YAW_CALIBRATING && !stationary)
  { clear_calibration(); reading.state = MPU_YAW_WAIT_STATIONARY; }
  if (reading.state == MPU_YAW_CALIBRATING && now_ms - calibration_ms > 10000U)
  { fault(MPU_FAULT_CALIBRATION); return; }
  if (now_ms - last_poll_ms < 5U) return;
  if (now_ms - last_poll_ms > reading.max_service_gap_ms)
    reading.max_service_gap_ms = now_ms - last_poll_ms;
  last_poll_ms = now_ms;
  if (!MpuBus_Read(0x3aU, &status, 1U) || !MpuBus_Read(0x72U, count_bytes, 2U))
  { fault(MPU_FAULT_BUS); return; }
  count = (uint16_t)((uint16_t)count_bytes[0] * 256U + count_bytes[1]);
  /* 8 frames is a per-call work budget, not the hardware FIFO capacity.
     Preserve complete history across calls; never publish a backlogged yaw
     as a current reading. A partial newest packet stays for the next poll. */
  if (count > reading.peak_fifo_bytes) reading.peak_fifo_bytes = count;
  if ((status & 0x10U) || count >= 1024U)
  { fault(MPU_FAULT_FIFO); return; }
  frames = count / FRAME_BYTES;
  reading.pending_frames = frames;
  if (frames > MAX_FRAMES) { ++reading.backlog_events; frames = MAX_FRAMES; }
  if (!frames)
  {
    /* Empty/partial FIFO makes READY false after 30 ms, but is recoverable.
       A sensor that produces no complete frames for 250 ms is a hard fault. */
    if (now_ms - reading.last_sample_ms > 250U) fault(MPU_FAULT_STALE);
    if (reading.state == MPU_YAW_WAIT_STATIONARY && stationary)
    { reading.state = MPU_YAW_CALIBRATING; calibration_ms = now_ms; }
    return;
  }
  if (!MpuBus_Read(0x74U, bytes, (uint16_t)(frames * FRAME_BYTES)))
  { fault(MPU_FAULT_BUS); return; }
  reading.pending_frames -= frames;
  reading.last_sample_ms = now_ms - (uint32_t)reading.pending_frames * 10U;
  for (i = 0; i < frames && reading.state != MPU_YAW_FAULT; ++i)
    sample(bytes + i * FRAME_BYTES, stationary);
  /* Frames captured before the STOP observation were discarded in WAIT.
     Start a new calibration only after that history has been drained. */
  if (reading.state == MPU_YAW_WAIT_STATIONARY && stationary && !reading.pending_frames)
  { reading.state = MPU_YAW_CALIBRATING; calibration_ms = now_ms; }
}
void MpuYaw_Refresh(uint32_t now_ms)
{ if (reading.state == MPU_YAW_READY) MpuYaw_Task(now_ms, 0U); }
void MpuYaw_GetReading(MpuYawReading *out) { if (out) *out = reading; }
uint8_t MpuYaw_IsReady(uint32_t now_ms)
{
  return reading.state == MPU_YAW_READY && ready_sample && !reading.pending_frames &&
      now_ms - reading.last_sample_ms <= 30U;
}
