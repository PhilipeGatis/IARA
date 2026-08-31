/**
 * Tiny fetch wrapper for the controller's REST API.
 *
 * Returns the parsed body so callers can await it and react to the result.
 * A network failure must be visible: the device lives on the local network and
 * the phone drops off it constantly, so a silently swallowed request would look
 * exactly like a successful one — dangerous when the command was "start a water
 * change" or "stop the pump".
 */
/**
 * Marks a request as coming from this page.
 *
 * The controller rejects any mutating request without it. A browser will not
 * attach a custom header to a cross-origin request without first asking
 * permission via a CORS preflight, and the firmware answers no preflight — so a
 * page on some other site cannot forge "start a water change" at the
 * controller's LAN address just because the owner's phone happens to be on the
 * same network with the dashboard open.
 */
export const REQUEST_HEADER = 'X-IARA-Request';

export const api = async (method: string, url: string, body?: unknown) => {
  const headers: Record<string, string> = { [REQUEST_HEADER]: '1' };
  if (body) headers['Content-Type'] = 'application/json';

  try {
    const r = await fetch(url, {
      method,
      headers,
      body: body ? JSON.stringify(body) : undefined,
    });
    const d = await r.json();
    if (d.error) alert(d.error);
    return d;
  } catch (e) {
    console.error(e);
    alert(netErrorMsg);
    return null;
  }
};

/** Set by <AppContent> so `api` can report failures in the active language. */
let netErrorMsg = 'Connection to the controller failed. The command was NOT sent.';
export const setNetErrorMsg = (m: string) => {
  netErrorMsg = m;
};

/**
 * The controller's clock is local time carried in a field named "epoch".
 *
 * TimeManager syncs NTP with the Brasilia offset already applied and writes
 * that into the RTC, so `unixtime()` — and therefore every timestamp the
 * firmware stores or reports — runs three hours behind real UTC.
 *
 * A browser epoch is real UTC, so sending one straight to /api/schedule stamped
 * a last run three hours in the *future* for the device. main.cpp reads a future
 * timestamp as a wrong clock and clears it, so "Foi feita agora" silently became
 * "nunca" within the minute, and the next water change went back to being due
 * immediately.
 *
 * Both helpers use the browser's own offset, which is right whenever the phone
 * and the aquarium are in the same timezone — and the dashboard is only reachable
 * from the tank's network.
 */
export const deviceEpochNow = () =>
  Math.floor(Date.now() / 1000) - new Date().getTimezoneOffset() * 60;

/** The real instant a timestamp from the controller refers to. */
export const dateFromDeviceEpoch = (epoch: number) =>
  new Date((epoch + new Date().getTimezoneOffset() * 60) * 1000);
