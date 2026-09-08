#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mpu6050_yaw.h"
#include "gyro_turn.h"
#include "line_bypass_turn.h"
#include "drive_base.h"
#include "angle_mode_example.h"

static uint32_t now;
static uint8_t fifo[1024], regs[256], bad_bus, overflow;
static uint16_t fifo_size;
static DriveBaseTelemetry drive;
static int32_t largest_cps, smallest_cps;
uint32_t HAL_GetTick(void) { return now; }
uint8_t MpuBus_Init(void) { return !bad_bus; }
uint8_t MpuBus_Write(uint8_t reg, uint8_t value)
{ regs[reg] = value; return !bad_bus; }
uint8_t MpuBus_Read(uint8_t reg, uint8_t *data, uint16_t length)
{
  if (bad_bus) return 0;
  if (reg == 0x3a) { *data = overflow ? 0x10 : 0; return 1; }
  if (reg == 0x72) { data[0] = (uint8_t)(fifo_size >> 8); data[1] = (uint8_t)fifo_size; return 1; }
  if (reg == 0x74)
  {
    assert(length <= fifo_size);
    memcpy(data, fifo, length);
    fifo_size = (uint16_t)(fifo_size - length);
    memmove(fifo, fifo + length, fifo_size);
    return 1;
  }
  assert(length == 1); *data = regs[reg]; return 1;
}
void DriveBase_GetTelemetry(DriveBaseTelemetry *out) { *out = drive; }
void DriveBase_Task(uint32_t time) { (void)time; }
void DriveBase_Stop(DriveStopMode mode)
{ (void)mode; drive.mode = DRIVE_BASE_STOPPED; memset(drive.requested_cps, 0, sizeof(drive.requested_cps)); }
void DriveBase_SetLineFaultObservation(uint8_t en, uint8_t sensors, uint8_t recovery)
{ assert(en == 0); (void)sensors; (void)recovery; }
void DriveBase_PrepareLineTurnAssist(int32_t left, int32_t right)
{ assert(left == -right); }
void DriveBase_SetSideCps(int32_t left, int32_t right)
{
  int32_t speed = left < 0 ? -left : left;
  assert(left == -right); assert(speed >= 1412 && speed <= 3600);
  if (speed > largest_cps) largest_cps = speed;
  if (speed < smallest_cps) smallest_cps = speed;
  drive.requested_cps[0] = drive.requested_cps[1] = left;
  drive.requested_cps[2] = drive.requested_cps[3] = right;
  drive.mode = DRIVE_BASE_SPEED;
}
static void put16(uint8_t *p, int32_t value)
{ uint16_t v = (uint16_t)value; p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void queue(unsigned frames, int32_t z, int32_t az)
{
  unsigned i;
  assert(frames <= 85);
  memset(fifo, 0, sizeof(fifo));
  fifo_size = (uint16_t)(frames * 12);
  for (i = 0; i < frames; ++i) { put16(fifo + i * 12 + 4, az); put16(fifo + i * 12 + 10, z); }
}
static void feed(unsigned frames, int32_t z, uint8_t stationary)
{
  queue(frames, z, 16384); now += frames * 10;
  MpuYaw_Task(now, stationary);
}
static MpuYawReading read_imu(void) { MpuYawReading r; MpuYaw_GetReading(&r); return r; }
static void boot(uint32_t start)
{
  GyroTurn_Stop(); memset(&drive, 0, sizeof(drive)); assert(GyroTurn_ClearFault());
  memset(regs, 0, sizeof(regs)); regs[0x75] = 0x68;
  bad_bus = overflow = 0; fifo_size = 0; now = start;
  MpuYaw_Init(now); now += 100; MpuYaw_Task(now, 1);
  now += 100; MpuYaw_Task(now, 1);
  assert(read_imu().state == MPU_YAW_CALIBRATING);
  assert(regs[0x19] == 9 && regs[0x1b] == 8 && regs[0x23] == 0x78);
  largest_cps = 0; smallest_cps = 9999;
}
static void ready(void)
{
  unsigned i;
  boot(0);
  for (i = 0; i < 200; ++i) feed(1, 100, 1);
  assert(MpuYaw_IsReady(now)); assert(read_imu().bias_milliraw == 100000);
}
static void test_yaw(void)
{
  unsigned i;
  ready();
  for (i = 0; i < 100; ++i) feed(1, 6650, 0);
  assert(read_imu().yaw_mdeg == 100000);
  for (i = 0; i < 100; ++i) feed(1, -6450, 0);
  assert(read_imu().yaw_mdeg == 0);
  ready();
  for (i = 0; i < 20; ++i) feed(5, 6650, 0);
  assert(read_imu().yaw_mdeg == 100000);
  assert(read_imu().rate_mdeg_s == 100000);
  boot(UINT32_MAX - 1000U);
  for (i = 0; i < 200; ++i) feed(1, 100, 1);
  assert(MpuYaw_IsReady(now));
  feed(1, 6650, 0); assert(read_imu().yaw_mdeg == 1000);
}
static void test_calibration_and_faults(void)
{
  unsigned i;
  boot(0); for (i = 0; i < 199; ++i) feed(1, 100, 1);
  assert(!MpuYaw_IsReady(now)); feed(1, 100, 0);
  assert(read_imu().calibration_samples == 0);
  queue(1, 100, -16384); now += 10; MpuYaw_Task(now, 1);
  assert(read_imu().calibration_samples == 0);
  for (i = 0; i < 200; ++i) feed(1, 100, 1);
  assert(MpuYaw_IsReady(now));
  feed(1, 32767, 0); assert(read_imu().fault == MPU_FAULT_RANGE);
  ready(); now += 31; assert(!MpuYaw_IsReady(now));
  now += 50; MpuYaw_Task(now, 1); assert(read_imu().fault == 0);
  now += 170; MpuYaw_Task(now, 1); assert(read_imu().fault == MPU_FAULT_STALE);
  ready(); overflow = 1; feed(1, 100, 0); assert(read_imu().fault == MPU_FAULT_FIFO);
  ready(); fifo_size = 1024; now += 10; MpuYaw_Task(now, 0);
  assert(read_imu().fault == MPU_FAULT_FIFO);
  ready(); bad_bus = 1; feed(1, 100, 0); assert(read_imu().fault == MPU_FAULT_BUS);
  boot(0); regs[0x75] = 0; MpuYaw_Init(now); now += 100; MpuYaw_Task(now, 1);
  assert(read_imu().fault == MPU_FAULT_ID);
  boot(0); for (i = 0; i < 1001; ++i) feed(1, 1000, 1);
  assert(read_imu().fault == MPU_FAULT_CALIBRATION);
  /* Less than a full packet does not refresh freshness or consume bytes. */
  ready(); fifo_size = 11; now += 10; MpuYaw_Task(now, 0);
  assert(fifo_size == 11); assert(read_imu().last_sample_ms == now - 10);
}
static void test_turn(int direction)
{
  unsigned i;
  ready();
  assert(LineBypassTurn_Start(direction * 90000, 2500));
  assert(drive.requested_cps[0] * direction < 0);
  assert(drive.requested_cps[2] * direction > 0);
  for (i = 0; i < 150 && LineBypassTurn_GetState() == LINE_BYPASS_TURN_RUNNING; ++i)
  {
    feed(1, 100 + (drive.mode == DRIVE_BASE_SPEED ? direction * 6550 : 0), 0);
    LineBypassTurn_Task();
  }
  assert(LineBypassTurn_GetState() == LINE_BYPASS_TURN_DONE);
  assert(LineBypassTurn_GetAchievedAngleMdeg() * direction >= 86000);
  assert(LineBypassTurn_GetAchievedAngleMdeg() * direction <= 94000);
  assert(largest_cps == 2500 && smallest_cps < 2000);
  assert(drive.mode == DRIVE_BASE_STOPPED);
}
static void test_turn_faults(void)
{
  unsigned i;
  ready(); assert(!GyroTurn_Start(INT32_MIN, 2500)); assert(!GyroTurn_Start(0, 2500));
  assert(!GyroTurn_Start(90000, 4000)); assert(GyroTurn_Start(90000, 2500));
  now += 31; GyroTurn_Task(); assert(GyroTurn_GetFault() == GYRO_TURN_DATA_GAP);
  assert(drive.mode == DRIVE_BASE_STOPPED); GyroTurn_Stop();
  assert(!GyroTurn_Start(90000, 2500)); /* STOP retains fault */
  ready(); assert(GyroTurn_Start(90000, 2500));
  drive.mode = DRIVE_BASE_STOPPED; GyroTurn_Task();
  assert(GyroTurn_GetFault() == GYRO_TURN_DRIVE && drive.mode == DRIVE_BASE_STOPPED);
  ready(); assert(GyroTurn_Start(90000, 2500));
  for (i = 0; i < 4; ++i) { feed(1, -6450, 0); GyroTurn_Task(); }
  assert(GyroTurn_GetFault() == GYRO_TURN_DIRECTION);
  ready(); assert(GyroTurn_Start(90000, 2500));
  for (i = 0; i < 120; ++i) { feed(1, 100, 0); GyroTurn_Task(); }
  assert(GyroTurn_GetFault() == GYRO_TURN_NO_PROGRESS);
  ready(); assert(GyroTurn_Start(90000, 2500)); assert(GyroTurn_RequestStop());
  for (i = 0; i < 14; ++i) { feed(1, 100, 0); GyroTurn_Task(); }
  assert(GyroTurn_GetState() == GYRO_TURN_DONE && GyroTurn_GetAchievedAngleMdeg() == 0);
  ready(); assert(GyroTurn_Start(90000, 2500)); GyroTurn_Stop();
  for (i = 0; i < 20; ++i) { feed(1, 6650, 0); GyroTurn_Task(); }
  assert(GyroTurn_GetState() == GYRO_TURN_IDLE && drive.mode == DRIVE_BASE_STOPPED);
  ready(); assert(GyroTurn_Start(90000, 2500));
  /* Drive reports no fault, but yaw overshoots 5 deg after braking. */
  for (i = 0; i < 88; ++i) { feed(1, 6650, 0); GyroTurn_Task(); }
  assert(drive.mode == DRIVE_BASE_STOPPED);
  for (i = 0; i < 10; ++i) { feed(1, 6650, 0); GyroTurn_Task(); }
  for (i = 0; i < 14; ++i) { feed(1, 100, 0); GyroTurn_Task(); }
  assert(GyroTurn_GetFault() == GYRO_TURN_ACCURACY);
  ready(); assert(GyroTurn_Start(90000, 2500)); drive.fault_mask = 1; GyroTurn_Task();
  assert(GyroTurn_GetFault() == GYRO_TURN_DRIVE);
}
static void test_reusable_example(void)
{
  int direction;
  unsigned i;
  for (direction = -1; direction <= 1; direction += 2)
  {
    AngleModeExample_Exit(); ready();
    assert(AngleModeExample_Start(direction * 90000, 2500));
    assert(!AngleModeExample_Start(45000, 2500));
    for (i = 0; i < 150; ++i)
    {
      feed(1, 100 + (drive.mode == DRIVE_BASE_SPEED ? direction * 6550 : 0), 0);
      AngleModeExample_Task();
    }
    assert(AngleModeExample_GetState() == ANGLE_EXAMPLE_DONE);
    assert(AngleModeExample_GetAngleMdeg() * direction >= 86000);
    assert(drive.mode == DRIVE_BASE_STOPPED);
  }
  AngleModeExample_Exit(); ready();
  assert(!AngleModeExample_Start(90000, 5000));
  assert(AngleModeExample_GetState() == ANGLE_EXAMPLE_IDLE);
  assert(AngleModeExample_Start(90000, 2500));
  AngleModeExample_Exit(); AngleModeExample_Task();
  assert(AngleModeExample_GetState() == ANGLE_EXAMPLE_IDLE && drive.mode == DRIVE_BASE_STOPPED);
  assert(AngleModeExample_Start(90000, 2500));
  now += 31; AngleModeExample_Task();
  assert(AngleModeExample_GetState() == ANGLE_EXAMPLE_FAULT);
  assert(!AngleModeExample_Start(90000, 2500));
  assert(GyroTurn_GetFault() == GYRO_TURN_DATA_GAP);
  AngleModeExample_Exit();
  puts("PASS: reusable mode example, mirrored completion, rejected start, cancellation, fault latch and no automatic restart");
}
static void test_delayed_service_and_recovery(void)
{
  unsigned i;
  uint32_t samples;
  ready(); assert(GyroTurn_Start(90000, 2500));
  queue(3, 6650, 16384); now += 35; GyroTurn_Task();
  assert(GyroTurn_GetState() == GYRO_TURN_RUNNING && !GyroTurn_GetFault());
  assert(read_imu().yaw_mdeg == 3000 && fifo_size == 0);
  /* A delayed loop must drain valid history before checking age. */
  queue(8, 6650, 16384); now += 81; GyroTurn_Task();
  assert(GyroTurn_GetState() == GYRO_TURN_RUNNING);
  assert(read_imu().yaw_mdeg == 11000);
  samples = read_imu().samples;
  MpuYaw_Refresh(now); MpuYaw_Refresh(now);
  assert(read_imu().samples == samples);
  /* Work is bounded to eight frames; incomplete catch-up is never READY. */
  ready(); queue(32, 6650, 16384); now += 320; MpuYaw_Task(now, 0);
  assert(read_imu().pending_frames == 24 && fifo_size == 288);
  assert(read_imu().fault == 0 && !MpuYaw_IsReady(now));
  for (i = 0; i < 3; ++i) { now += 5; MpuYaw_Refresh(now); }
  assert(MpuYaw_IsReady(now) && read_imu().yaw_mdeg == 32000);
  assert(read_imu().samples == 232 && read_imu().peak_fifo_bytes == 384);
  assert(read_imu().max_service_gap_ms == 320 && read_imu().backlog_events == 3);
  /* Temporary age failure aborts this turn. Fresh data may clear only this
     fault while stopped; neither clearing nor new samples start the motors. */
  ready(); assert(GyroTurn_Start(90000, 2500));
  now += 31; GyroTurn_Task();
  assert(GyroTurn_GetFault() == GYRO_TURN_DATA_GAP);
  assert(!GyroTurn_ClearTransientFault());
  feed(1, 100, 0); drive.mode = DRIVE_BASE_SPEED;
  assert(!GyroTurn_ClearTransientFault());
  drive.mode = DRIVE_BASE_STOPPED; drive.fault_mask = 1;
  assert(!GyroTurn_ClearTransientFault());
  drive.fault_mask = 0;
  assert(GyroTurn_ClearTransientFault());
  GyroTurn_Task(); assert(drive.mode == DRIVE_BASE_STOPPED);
  assert(GyroTurn_GetState() == GYRO_TURN_IDLE && read_imu().calibration_samples == 200);
  assert(GyroTurn_Start(-90000, 2500));
  bad_bus = 1; now += 10; GyroTurn_Task();
  assert(GyroTurn_GetFault() == GYRO_TURN_SENSOR && !GyroTurn_ClearTransientFault());
  /* Running mode 2 before calibration is an explicit wait, not a ten-second
     permanent fault. Buffered pre-STOP samples cannot become calibration. */
  boot(0);
  for (i = 0; i < 1500; ++i) feed(1, 100, 0);
  assert(read_imu().state == MPU_YAW_WAIT_STATIONARY && !read_imu().fault);
  queue(16, 100, 16384); now += 160; MpuYaw_Task(now, 1);
  assert(read_imu().state == MPU_YAW_WAIT_STATIONARY);
  now += 5; MpuYaw_Task(now, 1);
  assert(read_imu().state == MPU_YAW_CALIBRATING && read_imu().calibration_samples == 0);
  for (i = 0; i < 200; ++i) feed(1, 100, 1);
  assert(MpuYaw_IsReady(now) && !read_imu().fault);
  puts("PASS: delayed consumer refresh, budgeted FIFO catch-up, transient abort/rearm and stationary calibration wait");
}
int main(void)
{
  test_yaw(); test_calibration_and_faults(); test_turn(1); test_turn(-1); test_turn_faults();
  test_reusable_example();
  test_delayed_service_and_recovery();
  puts("PASS: FIFO yaw, calibration, faults, mirrored bypass turns, STOP and settled-angle checks");
  return 0;
}
