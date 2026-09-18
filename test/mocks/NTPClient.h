#pragma once
// ============================================================================
// NTPClient.h Mock for Native Unit Tests
// ============================================================================

#include "Arduino.h"
#include "WiFiUdp.h"

class NTPClient {
public:
  NTPClient(WiFiUDP &udp, const char *server = "pool.ntp.org", long offset = 0)
      : _offset(offset), _epoch(0) {}

  void begin() {}
  void setTimeOffset(long offset) { _offset = offset; }
  bool update() { return forceUpdate(); }

  /// Whether a packet came back. The real client answers false on timeout and
  /// leaves the cached epoch untouched, which is the case that matters: the
  /// epoch on its own never reports failure.
  bool forceUpdate() { return _updateOk; }
  bool isTimeSet() const { return _updateOk && _epoch != 0; }

  /// Deliberately the real arithmetic, wrap and all: with _epoch left at zero
  /// and a negative offset this underflows exactly as it does on the device.
  unsigned long getEpochTime() { return _epoch + _offset; }
  String getFormattedTime() { return String("12:00:00"); }

  // Mock control
  void mock_setEpoch(unsigned long epoch) { _epoch = epoch; }
  void mock_setUpdateOk(bool ok) { _updateOk = ok; }

private:
  long _offset;
  unsigned long _epoch;
  bool _updateOk = true;
};
