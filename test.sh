#!/usr/bin/env bash
set -euo pipefail

PORTAL_DEST="org.freedesktop.portal.Desktop"
PORTAL_PATH="/org/freedesktop/portal/desktop"
SCREENCAST_IF="org.freedesktop.portal.ScreenCast"

TMP_MONITOR="$(mktemp)"
gdbus monitor --session --dest "$PORTAL_DEST" >"$TMP_MONITOR" &
MONITOR_PID=$!

cleanup() {
  kill "$MONITOR_PID" 2>/dev/null || true
  rm -f "$TMP_MONITOR"
}
trap cleanup EXIT

SESSION_TOKEN="s1_$$"
HANDLE_CREATE="req1_$$"
HANDLE_SELECT="hs$$"
HANDLE_START="ht$$"

echo "[*] CreateSession..."

# The session path is predictable based on the session_handle_token
# Format: /org/freedesktop/portal/desktop/session/{sender_id}/{session_handle_token}
# We need to construct it from the sender name

# Get our unique bus name (not the portal's name, but OUR connection name)
# We'll use a simpler approach: call Hello to get our own name
SENDER=$(dbus-send --session --print-reply --dest=org.freedesktop.DBus \
  /org/freedesktop/DBus org.freedesktop.DBus.Hello 2>/dev/null | \
  grep -o ':[0-9.]*' | head -1)

echo "[+] Sender: $SENDER"

# Call CreateSession (using predictable tokens)
gdbus call --session \
  --dest org.freedesktop.portal.Desktop \
  --object-path /org/freedesktop/portal/desktop \
  --method org.freedesktop.portal.ScreenCast.CreateSession \
  "{'session_handle_token': <'$SESSION_TOKEN'>, 'handle_token': <'$HANDLE_CREATE'>}" >/dev/null

# Construct the session handle path
# Convert sender name (e.g., :1.123) to path format (e.g., 1_123)
SENDER_PATH=$(echo "$SENDER" | sed 's/^://; s/\./_/g')
SESSION_HANDLE="/org/freedesktop/portal/desktop/session/${SENDER_PATH}/${SESSION_TOKEN}"

echo "[+] Session handle: $SESSION_HANDLE"

# Verify the session exists
if ! timeout 2 gdbus introspect --session \
     --dest org.freedesktop.portal.Desktop \
     --object-path "$SESSION_HANDLE" &>/dev/null; then
  echo "[!] Failed to verify session handle"
  exit 1
fi

echo "[*] SelectSources (monitor)..."
gdbus call --session \
  --dest "$PORTAL_DEST" \
  --object-path "$PORTAL_PATH" \
  --method "$SCREENCAST_IF.SelectSources" \
  "$SESSION_HANDLE" \
  "{'types': <uint32 1>, 'multiple': <false>, 'cursor_mode': <uint32 2>, 'handle_token': <'$HANDLE_SELECT'>}" >/dev/null

echo "[*] Start session (you will see a Wayland share dialog)..."
gdbus call --session \
  --dest "$PORTAL_DEST" \
  --object-path "$PORTAL_PATH" \
  --method "$SCREENCAST_IF.Start" \
  "$SESSION_HANDLE" \
  "" \
  "{'handle_token': <'$HANDLE_START'>}" >/dev/null

echo "[*] Please choose a monitor and confirm in the dialog..."
while ! grep -q "$HANDLE_START" "$TMP_MONITOR"; do sleep 0.1; done

NODE_ID=$(
  grep "$HANDLE_START" "$TMP_MONITOR" \
    | grep -o "uint32 [0-9]\+" \
    | awk '{print $2}' \
    | head -n1
)

echo "[+] PipeWire node id: $NODE_ID"

echo "[*] OpenPipeWireRemote to get FD..."
PW_FD_LINE=$(
  gdbus call --session \
    --dest "$PORTAL_DEST" \
    --object-path "$PORTAL_PATH" \
    --method "$SCREENCAST_IF.OpenPipeWireRemote" \
    "$SESSION_HANDLE" \
    "{}"
)

PW_FD_NUM=$(echo "$PW_FD_LINE" | grep -o "unix-fd [0-9]\+" | awk '{print $2}')

echo "[+] PipeWire FD (in this process): $PW_FD_NUM"

echo
echo "Use these in your PipeWire app:"
echo "  PW_FD=$PW_FD_NUM"
echo "  NODE_ID=$NODE_ID"
