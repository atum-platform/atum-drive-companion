#!/usr/bin/env bash
# Real Secret Service for headless Linux tests; synthetic entries only.
set -euo pipefail
test -n "${DBUS_SESSION_BUS_ADDRESS:-}"
test "$#" -gt 0
umask 077
keyring_fixture_dir=$(mktemp -d "${TMPDIR:-/tmp}/atum-ci-keyring.XXXXXXXX")
keyring_fixture_pid=
cleanup() {
    keyring_fixture_status=$?
    if test "$keyring_fixture_status" -ne 0; then
        printf 'Disposable Secret Service preflight failed (%s)\n' "$keyring_fixture_status" >&2
        cat "$keyring_fixture_dir/daemon.log" >&2 || true
    fi
    if test -n "$keyring_fixture_pid"; then
        kill "$keyring_fixture_pid" 2>/dev/null || true
        wait "$keyring_fixture_pid" 2>/dev/null || true
    fi
    rm -rf -- "$keyring_fixture_dir"
}
trap cleanup EXIT
export XDG_DATA_HOME="$keyring_fixture_dir/data"
export XDG_CONFIG_HOME="$keyring_fixture_dir/config"
export XDG_RUNTIME_DIR="$keyring_fixture_dir/runtime"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_RUNTIME_DIR"
keyring_fixture_daemon=$(command -v gnome-keyring-daemon)
# Preserve evidence of the packaged daemon's memory-lock requirement. The Linux
# job grants only IPC_LOCK so this real daemon can execute and drop capabilities.
if command -v getcap >/dev/null; then
    getcap "$keyring_fixture_daemon"
fi
printf %s 'disposable-ci-keyring-password' | "$keyring_fixture_daemon" --foreground --unlock --components=secrets --control-directory="$XDG_RUNTIME_DIR" >"$keyring_fixture_dir/daemon.log" 2>&1 &
keyring_fixture_pid=$!
timeout 15 gdbus wait --session org.freedesktop.secrets
printf %s 'synthetic-secret-service-probe' | timeout 15 secret-tool store --label='Atum CI probe' atum-ci probe
test "$(timeout 15 secret-tool lookup atum-ci probe)" = synthetic-secret-service-probe
secret-tool clear atum-ci probe
"$@"
