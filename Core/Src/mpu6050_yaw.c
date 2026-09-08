#include "mpu6050_yaw.h"
#include <string.h>

/* 100 Hz FIFO, accel +/-2 g, gyro +/-500 deg/s (65.5 LSB/deg/s).
   FIFO order: AX AY AZ GX GY GZ, big endian, no temperature. */
#define FRAME_BYTES 12U
#define MAX_FRAMES 8U
#define CAL_SAMPLES 200U
static MpuYawReading reading;
static uint32_t stage_ms, last_poll_ms, calibration_ms;
static uint8_t init_stage;
static int64_t bias_sum, yaw_udeg;
static int16_t cal_min[3], cal_max[3];

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
  reading.rate_mdeg_s = 0;
}
static void clear_calibration(void)
{
  reading.calibration_samples = 0;
  bias_sum = 0;
}
void MpuYaw_Init(uint32_t now_ms)
{
  memset(&reading, 0, sizeof(reading));
  yaw_udeg = bias_sum = 0;
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
  ++reading.samples;
  if (reading.state == MPU_YAW_WAIT_STATIONARY) return;
  if (reading.state == MPU_YAW_CALIBRATING)
  {
    /* Reject a tilted/upside-down mounting and visible motion. Constant very
       slow hand rotation is not distinguishable from bias: keep car still. */
    if (!stationary || magnitude(ax) > 4500 || magnitude(ay) > 4500 ||
        az * MPU6050_YAW_SIGN < 14000 || az * MPU6050_YAW_SIGN > 18500 ||
        magnitude(gyro[0]) > 196 || magnitude(gyro[1]) > 196 ||
        magnitude(gyro[2]) > 196)
    { clear_calibration(); return; }
    for (i = 0; i < 3; ++i)
    {
      if (!reading.calibration_samples) cal_min[i] = cal_max[i] = gyro[i];
      if (gyro[i] < cal_min[i]) cal_min[i] = gyro[i];
      if (gyro[i] > cal_max[i]) cal_max[i] = gyro[i];
      if (cal_max[i] - cal_min[i] > 65) { clear_calibration(); return; }
    }
    bias_sum += gyro[2];
    if (++reading.calibration_samples == CAL_SAMPLES)
    {
      reading.bias_milliraw = (int32_t)(bias_sum * 1000 / CAL_SAMPLES);
      reading.state = MPU_YAW_READY;
    }
    return;
  }
  if (magnitude(gyro[0]) >= 32000 || magnitude(gyro[1]) >= 32000 ||
      magnitude(gyro[2]) >= 32000)
  { fault(MPU_FAULT_RANGE); return; }
  {
    int32_t corrected = (gyro[2] * 1000 - reading.bias_milliraw) * MPU6050_YAW_SIGN;
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
  if (reading.state == MPU_YAW_FAULT) return;
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
    reading.state = stationary ? MPU_YAW_CALIBRATING : MPU_YAW_WAIT_STATIONARY;
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
  return reading.state == MPU_YAW_READY && !reading.pending_frames &&
      now_ms - reading.last_sample_ms <= 30U;
}
