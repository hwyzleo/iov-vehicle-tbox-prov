#!/bin/bash
# Smoke test for tbox-prov service IPC/config.
# Exit 0 = pass, non-zero = fail.
#
# TBOX-PROV-DSN-CR-009 §10.2: PROV 特有 IPC/配置冒烟入口。
# 不写入真实 VIN、密钥或生产身份；仅验证服务启动、socket 和配置可用性。

set -euo pipefail

# --- Config file existence ---
CONFIG_FILE="/etc/tbox/conf.d/prov.yaml"
if [ ! -f "${CONFIG_FILE}" ]; then
    echo "FAIL: config file ${CONFIG_FILE} not found"
    exit 1
fi

# --- IPC socket existence ---
SOCKET_PATH="/tmp/tbox-prov.sock"
if [ ! -S "${SOCKET_PATH}" ]; then
    echo "FAIL: IPC socket ${SOCKET_PATH} not found"
    exit 1
fi

# --- Service lifecycle: restart and verify socket recreation ---
if ! systemctl restart tbox-prov.service; then
    echo "FAIL: could not restart tbox-prov.service"
    exit 1
fi

# Wait for socket to reappear (up to 5 seconds)
for i in $(seq 1 50); do
    if [ -S "${SOCKET_PATH}" ]; then
        break
    fi
    sleep 0.1
done

if [ ! -S "${SOCKET_PATH}" ]; then
    echo "FAIL: IPC socket ${SOCKET_PATH} not recreated after restart"
    exit 1
fi

# --- Service status ---
if ! systemctl is-active --quiet tbox-prov.service; then
    echo "FAIL: tbox-prov.service not active after restart"
    exit 1
fi

# --- Graceful shutdown and verify socket cleanup ---
systemctl stop tbox-prov.service

# Wait for socket cleanup (up to 3 seconds)
for i in $(seq 1 30); do
    if [ ! -e "${SOCKET_PATH}" ]; then
        break
    fi
    sleep 0.1
done

if [ -e "${SOCKET_PATH}" ]; then
    echo "WARN: IPC socket ${SOCKET_PATH} still exists after stop (may be cleaning up)"
fi

# Restart for post-smoke state
systemctl start tbox-prov.service

echo "PASS: tbox-prov IPC/config smoke test (config, socket, restart, stop, start)"
exit 0
