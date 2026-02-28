#!/usr/bin/env python3
"""
compile_watch.py — UBT / LiveCoding compile status monitor.

Usage:
    python compile_watch.py [--host 127.0.0.1] [--port 55557] [--interval 5]

Polls the UnrealMCP server for LiveCoding status and reports changes to stdout.
Can be run as a standalone background monitor during development.
"""

import argparse
import json
import socket
import sys
import time
from datetime import datetime

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 55557
DEFAULT_INTERVAL = 5  # seconds


def send_command(host: str, port: int, command: str, params: dict = None, timeout: float = 5.0):
    """Send a single MCP command and return the parsed JSON response."""
    payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            s.sendall(payload)
            chunks = []
            s.settimeout(timeout)
            while True:
                chunk = s.recv(4096)
                if not chunk:
                    break
                chunks.append(chunk)
                try:
                    data = b"".join(chunks)
                    return json.loads(data.decode("utf-8"))
                except json.JSONDecodeError:
                    continue
    except Exception as e:
        return {"status": "error", "error": str(e)}


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S")


def main():
    parser = argparse.ArgumentParser(description="UnrealMCP compile status monitor")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--interval", type=float, default=DEFAULT_INTERVAL,
                        help="Poll interval in seconds")
    args = parser.parse_args()

    print(f"[{ts()}] compile_watch starting — {args.host}:{args.port} (interval={args.interval}s)")
    print("Press Ctrl-C to stop.\n")

    last_status = None

    while True:
        # 1. Check port
        try:
            with socket.create_connection((args.host, args.port), timeout=1.0):
                port_open = True
        except OSError:
            port_open = False

        if not port_open:
            status_str = "OFFLINE (port closed)"
        else:
            # 2. Ping
            ping = send_command(args.host, args.port, "ping")
            if ping.get("status") == "error":
                status_str = f"OFFLINE (ping failed: {ping.get('error', '?')})"
            else:
                # 3. LiveCoding status
                lc = send_command(args.host, args.port, "get_live_coding_status")
                if lc.get("status") == "error":
                    status_str = "ONLINE (live_coding_status unavailable)"
                else:
                    available = lc.get("live_coding_available", False)
                    lc_status = lc.get("status", "unknown")
                    status_str = f"ONLINE | LiveCoding={'yes' if available else 'no'} status={lc_status}"

        if status_str != last_status:
            print(f"[{ts()}] STATUS CHANGED → {status_str}")
            last_status = status_str
        else:
            print(f"[{ts()}] {status_str}", end="\r", flush=True)

        time.sleep(args.interval)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[compile_watch] Stopped.")
        sys.exit(0)
