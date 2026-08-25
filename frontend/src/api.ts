/**
 * Tiny fetch wrapper for the controller's REST API.
 *
 * Returns the parsed body so callers can await it and react to the result.
 * A network failure must be visible: the device lives on the local network and
 * the phone drops off it constantly, so a silently swallowed request would look
 * exactly like a successful one — dangerous when the command was "start a water
 * change" or "stop the pump".
 */
export const api = async (method: string, url: string, body?: unknown) => {
  const headers: Record<string, string> = {};
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
