#!/bin/bash
# Health check for tbox-prov service.
# Exit 0 = healthy, non-zero = unhealthy.
#
# TBOX-PROV-DSN-CR-009 §10.2: PROV 特有健康检查入口。

set -euo pipefail

# Check if the tbox-prov binary exists
if [ ! -x /usr/bin/tbox-prov ]; then
    echo "ERROR: tbox-prov binary not found at /usr/bin/tbox-prov"
    exit 1
fi

# Check if the systemd service is active
if ! systemctl is-active --quiet tbox-prov.service; then
    echo "ERROR: tbox-prov.service is not active"
    exit 1
fi

# Check if the IPC socket exists
SOCKET_PATH="/tmp/tbox-prov.sock"
if [ ! -S "${SOCKET_PATH}" ]; then
    echo "ERROR: IPC socket ${SOCKET_PATH} not found"
    exit 1
fi

echo "OK: tbox-prov service is healthy (binary, service, socket)"
exit 0
