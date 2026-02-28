#!/usr/bin/env python3
"""
debug_runner.py — Batch MCP command executor for debugging and validation.

Usage:
    python debug_runner.py [script.json] [--host 127.0.0.1] [--port 55557]
    python debug_runner.py --smoke-test

A 'script' is a JSON file containing an array of MCP command objects:
  [
    {"type": "ping", "params": {}},
    {"type": "get_current_level_name", "params": {}},
    {"type": "get_actors_in_level", "params": {}}
  ]

Results are printed to stdout as formatted JSON.
Exit code 0 = all commands succeeded, 1 = at least one error.
"""

import argparse
import json
import socket
import sys
import time
from datetime import datetime
from typing import Any, Dict, List

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 55557

# Built-in smoke test — verifies core MCP commands work
SMOKE_TEST_SCRIPT: List[Dict[str, Any]] = [
    {"type": "ping", "params": {}},
    {"type": "get_capabilities", "params": {}},
    {"type": "get_current_level_name", "params": {}},
    {"type": "get_world_settings", "params": {}},
    {"type": "get_project_settings", "params": {}},
    {"type": "get_live_coding_status", "params": {}},
    {"type": "get_engine_path", "params": {}},
    {"type": "get_actors_in_level", "params": {}},
]


def send_command(host: str, port: int, command: str, params: dict = None,
                 timeout: float = 10.0) -> Dict[str, Any]:
    """Send a single MCP command and return the parsed JSON response."""
    payload = json.dumps({"type": command, "params": params or {}}).encode("utf-8")
    try:
        with socket.create_connection((host, port), timeout=timeout) as s:
            s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            s.sendall(payload)
            chunks = []
            s.settimeout(timeout)
            while True:
                chunk = s.recv(8192)
                if not chunk:
                    break
                chunks.append(chunk)
                try:
                    return json.loads(b"".join(chunks).decode("utf-8"))
                except json.JSONDecodeError:
                    continue
        return {"status": "error", "error": "Connection closed without complete response"}
    except Exception as e:
        return {"status": "error", "error": str(e)}


def run_script(commands: List[Dict[str, Any]], host: str, port: int) -> int:
    """Execute a list of command dicts. Returns 0 if all succeed, 1 otherwise."""
    total = len(commands)
    passed = 0
    failed = 0

    print(f"\n{'='*60}")
    print(f"UnrealMCP Debug Runner — {host}:{port}")
    print(f"Running {total} command(s)...")
    print(f"{'='*60}\n")

    for i, cmd_obj in enumerate(commands, 1):
        cmd_type = cmd_obj.get("type", "unknown")
        cmd_params = cmd_obj.get("params", {})

        print(f"[{i:02d}/{total}] {cmd_type}", end=" ... ", flush=True)
        t0 = time.time()
        result = send_command(host, port, cmd_type, cmd_params)
        elapsed = time.time() - t0

        is_error = result.get("status") == "error" or result.get("success") is False
        status_label = "FAIL" if is_error else "OK"
        print(f"{status_label} ({elapsed:.2f}s)")

        if is_error:
            failed += 1
            err_msg = result.get("error") or result.get("message") or json.dumps(result)
            print(f"         ERROR: {err_msg}")
        else:
            passed += 1
            # Print a compact preview of the result
            preview = json.dumps(result, ensure_ascii=False)
            if len(preview) > 120:
                preview = preview[:117] + "..."
            print(f"         {preview}")

        print()

    print(f"{'='*60}")
    print(f"Results: {passed} passed, {failed} failed out of {total}")
    print(f"{'='*60}\n")
    return 0 if failed == 0 else 1


def main():
    parser = argparse.ArgumentParser(description="UnrealMCP batch debug runner")
    parser.add_argument("script", nargs="?", help="Path to JSON script file")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--smoke-test", action="store_true",
                        help="Run built-in smoke test instead of a script file")
    args = parser.parse_args()

    if args.smoke_test:
        commands = SMOKE_TEST_SCRIPT
    elif args.script:
        try:
            with open(args.script, "r", encoding="utf-8") as f:
                commands = json.load(f)
        except Exception as e:
            print(f"ERROR: Could not load script file: {e}")
            sys.exit(1)
    else:
        parser.print_help()
        sys.exit(1)

    exit_code = run_script(commands, args.host, args.port)
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
