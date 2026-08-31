#pragma once

#include "Config.h"

/// @file
/// Setpoint arithmetic for one water-change cycle.
///
/// Pulled out of WebManager::triggerTPA() so it can be tested. That function
/// lives inside a web handler and needs the whole async server to exist, which
/// is why the numbers that decide how much water moves were the least-tested
/// part of the system.

/// What one cycle will do, in centimetres of sensor distance and litres.
///
/// Distance grows as the water drops, so a level *above* the full mark means
/// the tank is short by the difference.
struct TpaPlan {
  bool ok = false;
  const char *refusal = nullptr; ///< why, when !ok

  float drainTargetCm = 0;  ///< drain until the sensor reads this
  float refillTargetCm = 0; ///< refill until the sensor reads this
  float drainLiters = 0;    ///< what the drain pump removes
  float refillLiters = 0;   ///< new water the reservoir must supply
};

/// Plans a cycle from the measured level.
///
/// The shortfall is water that has already left the tank, so a refill to the
/// absolute full mark replaces it. Subtracting it from the drain therefore
/// delivers exactly `requestedLiters` of new water and leaves the tank full,
/// rather than draining the full amount on top of a shortfall — or refusing to
/// run until someone tops it up by hand.
///
/// A tank above the mark works the same way in reverse: the excess is added to
/// the drain and the refill still stops at full. Evaporation and overfilling
/// both correct themselves over a cycle instead of accumulating.
///
/// @param currentCm      level now, as sensor distance
/// @param fullCm         the calibrated 100% mark, as sensor distance
/// @param requestedLiters new water the cycle should deliver
/// @param litersPerCm    tank footprint
/// @param reservoirAvailL usable reservoir volume, after its safety margin
/// @param minCm          the sensor's blind zone, below which a reading is not
///                       a level. Configurable per module, hence a parameter.
inline TpaPlan planTPA(float currentCm, float fullCm, float requestedLiters,
                       float litersPerCm, float reservoirAvailL,
                       float minCm = ULTRASONIC_MIN_DISTANCE_DEFAULT_CM) {
  TpaPlan p;

  if (currentCm < minCm || currentCm > ULTRASONIC_MAX_DISTANCE_CM) {
    p.refusal = "implausible level reading";
    return p;
  }
  if (fullCm <= 0) {
    p.refusal = "sensor 100% mark not calibrated";
    return p;
  }
  if (litersPerCm <= 0) {
    p.refusal = "aquarium dimensions not set";
    return p;
  }
  if (reservoirAvailL <= 0) {
    p.refusal = "reservoir safety margin exceeds its volume";
    return p;
  }

  p.refillLiters = requestedLiters;
  if (p.refillLiters > reservoirAvailL)
    p.refillLiters = reservoirAvailL; // the reservoir cannot give more

  const float deficitCm = currentCm - fullCm;
  const float refillCm = p.refillLiters / litersPerCm;
  const float cmToDrain = refillCm - deficitCm;

  if (cmToDrain <= 0) {
    // The shortfall alone already exceeds the change being asked for. Topping
    // the tank up delivers more new water than this cycle would, so draining
    // further is not what the tank needs.
    p.refusal = "tank is lower than the change would replace";
    return p;
  }

  p.drainTargetCm = currentCm + cmToDrain;
  p.refillTargetCm = fullCm;
  p.drainLiters = cmToDrain * litersPerCm;
  p.ok = true;
  return p;
}

// ---------------------------------------------------------------------------
// Distance <-> percentage
//
// The ultrasonic measures distance down to the surface, so the numbers only
// mean anything relative to the calibrated 100% mark. Leaving that offset out
// is not a rounding error: with a 32.8 cm tank whose mark sits at 8.2 cm, a
// full tank reads 75% and a "safe level" of 80% resolves to a distance 1.6 cm
// ABOVE the mark — a level the tank reaches only by overflowing. Anything
// gated on it can then never turn on.
//
// One conversion, used everywhere, so the three copies that disagreed cannot
// come back.
// ---------------------------------------------------------------------------

/// Water level as a percentage of the tank's usable height.
/// 100% is the calibrated full mark; 0% is `effHCm` below it.
inline float levelPercentFromDistance(float distCm, float fullCm,
                                      float effHCm) {
  if (effHCm <= 0)
    return 0;
  const float pct = ((fullCm + effHCm - distCm) / effHCm) * 100.0f;
  return pct < 0 ? 0 : (pct > 100.0f ? 100.0f : pct);
}

/// The sensor distance a given percentage corresponds to. Inverse of the above.
inline float distanceForLevelPercent(float pct, float fullCm, float effHCm) {
  if (effHCm <= 0)
    return fullCm;
  return fullCm + effHCm * (100.0f - pct) / 100.0f;
}
