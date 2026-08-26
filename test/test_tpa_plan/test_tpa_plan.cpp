// ============================================================================
// TPA setpoint arithmetic
//
// These numbers decide how much water leaves the aquarium. They used to live
// inside a web handler and were the least-tested part of the system.
// ============================================================================

#include "TpaPlan.h"
#include <unity.h>

// The tank on the bench: 60 x 30 cm footprint, 100% mark at 8.2 cm.
static constexpr float L_PER_CM = 1.8f;
static constexpr float FULL_CM = 8.2f;
static constexpr float RES_AVAIL = 18.0f;

void setUp() {}
void tearDown() {}

void test_full_tank_drains_exactly_the_change() {
  // 11.8 L of a 59 L tank is 6.556 cm.
  TpaPlan p = planTPA(FULL_CM, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL);
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.8f, p.refillLiters);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.8f, p.drainLiters);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, FULL_CM, p.refillTargetCm);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, FULL_CM + 6.556f, p.drainTargetCm);
}

void test_low_tank_drains_less_and_still_delivers_the_change() {
  // Three centimetres down: 5.4 L already gone.
  TpaPlan p = planTPA(FULL_CM + 3.0f, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL);
  TEST_ASSERT_TRUE(p.ok);

  // The drain removes only what is left after the shortfall...
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 11.8f - 5.4f, p.drainLiters);
  // ...but the refill still puts back the whole change, because it returns to
  // the absolute mark rather than to wherever the level happened to be.
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.8f, p.refillLiters);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, FULL_CM, p.refillTargetCm);

  // New water in equals the configured change, and the tank ends full.
  const float newWater = (p.drainTargetCm - p.refillTargetCm) * L_PER_CM;
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 11.8f, newWater);
}

void test_overfilled_tank_drains_more_and_still_ends_full() {
  // Two centimetres above the mark.
  TpaPlan p = planTPA(FULL_CM - 2.0f, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL);
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 11.8f + 3.6f, p.drainLiters);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, FULL_CM, p.refillTargetCm);
}

void test_shortfall_bigger_than_the_change_is_refused() {
  // Eight centimetres down is 14.4 L — more than the 11.8 L this cycle would
  // deliver. Draining further is not what the tank needs.
  TpaPlan p = planTPA(FULL_CM + 8.0f, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL);
  TEST_ASSERT_FALSE(p.ok);
  TEST_ASSERT_NOT_NULL(p.refusal);
}

void test_reservoir_caps_the_change() {
  // Ask for 30 L from an 18 L reservoir.
  TpaPlan p = planTPA(FULL_CM, FULL_CM, 30.0f, L_PER_CM, RES_AVAIL);
  TEST_ASSERT_TRUE(p.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, RES_AVAIL, p.refillLiters);
}

void test_uncalibrated_and_implausible_inputs_are_refused() {
  TEST_ASSERT_FALSE(planTPA(FULL_CM, 0.0f, 11.8f, L_PER_CM, RES_AVAIL).ok);
  TEST_ASSERT_FALSE(planTPA(FULL_CM, FULL_CM, 11.8f, 0.0f, RES_AVAIL).ok);
  TEST_ASSERT_FALSE(planTPA(FULL_CM, FULL_CM, 11.8f, L_PER_CM, 0.0f).ok);
  TEST_ASSERT_FALSE(planTPA(0.5f, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL).ok);
  TEST_ASSERT_FALSE(planTPA(999.0f, FULL_CM, 11.8f, L_PER_CM, RES_AVAIL).ok);
}

void test_percent_is_measured_from_the_calibrated_mark() {
  const float effH = 32.78f;

  // The mark is 100%, not whatever (effH - distance) happens to give. Omitting
  // the offset made a full tank read 75% here.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f,
                           levelPercentFromDistance(FULL_CM, FULL_CM, effH));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f,
                           levelPercentFromDistance(FULL_CM + effH, FULL_CM, effH));
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f,
                           levelPercentFromDistance(FULL_CM + effH / 2, FULL_CM, effH));
}

void test_a_safe_level_is_reachable_without_overflowing() {
  const float effH = 32.78f;

  // 80% must resolve to a distance BELOW the full mark, i.e. a level the tank
  // can actually be at. The old arithmetic gave 6.56 cm — 1.6 cm above the
  // 8.2 cm mark — so anything gated on it could never be allowed on.
  const float safe = distanceForLevelPercent(80.0f, FULL_CM, effH);
  TEST_ASSERT_TRUE_MESSAGE(safe > FULL_CM,
                           "safe level must sit below the full mark");
  TEST_ASSERT_FLOAT_WITHIN(0.05f, FULL_CM + effH * 0.2f, safe);

  // And the round trip holds.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 80.0f,
                           levelPercentFromDistance(safe, FULL_CM, effH));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_full_tank_drains_exactly_the_change);
  RUN_TEST(test_low_tank_drains_less_and_still_delivers_the_change);
  RUN_TEST(test_overfilled_tank_drains_more_and_still_ends_full);
  RUN_TEST(test_shortfall_bigger_than_the_change_is_refused);
  RUN_TEST(test_reservoir_caps_the_change);
  RUN_TEST(test_uncalibrated_and_implausible_inputs_are_refused);
  RUN_TEST(test_percent_is_measured_from_the_calibrated_mark);
  RUN_TEST(test_a_safe_level_is_reachable_without_overflowing);
  return UNITY_END();
}
