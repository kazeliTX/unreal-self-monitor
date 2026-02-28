"""
Log Tools for Unreal MCP.

Reads UE log files directly from the filesystem (crash-safe; no TCP required).
Parses the standard UE log format:
  [YYYY.MM.DD-HH.MM.SS:mmm][frame] Category: Verbosity: Message
"""

import logging
import os
import re
import glob
from typing import Dict, Any, Optional, List
from mcp.server.fastmcp import FastMCP, Context
from tools.base import make_error

logger = logging.getLogger("UnrealMCP")

# UE log line pattern:  [2025.01.01-12.00.00:000][  0]LogTemp: Warning: text
_LOG_RE = re.compile(
    r"^\[(?P<ts>[^\]]+)\]\[(?P<frame>[^\]]+)\](?P<category>[^:]+):(?P<verbosity>[^:]+)?:?\s*(?P<message>.*)$"
)

# Keywords that indicate critical issues
_ERROR_KEYWORDS = [
    "Error:", "error:", "Fatal:", "fatal:", "Assert:", "Assertion failed",
    "exception", "crash", "Access violation", "Unhandled", "LogOutputDevice: Error"
]
_WARNING_KEYWORDS = ["Warning:", "warning:"]


def _find_latest_log(project_dir: Optional[str] = None) -> Optional[str]:
    """Return the path to the most recent UE .log file."""
    if project_dir:
        log_dir = os.path.join(project_dir, "Saved", "Logs")
    else:
        # Best-effort: search common project locations
        log_dir = None
        candidates = [
            os.path.join(os.getcwd(), "Saved", "Logs"),
        ]
        for c in candidates:
            if os.path.isdir(c):
                log_dir = c
                break

    if not log_dir or not os.path.isdir(log_dir):
        return None

    log_files = glob.glob(os.path.join(log_dir, "*.log"))
    if not log_files:
        return None

    return max(log_files, key=os.path.getmtime)


def register_log_tools(mcp: FastMCP):
    """Register log analysis tools with the MCP server."""

    @mcp.tool()
    def get_log_file_path(
        ctx: Context,
        project_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Get the path to the most recent UE log file.

        Args:
            project_dir: Optional absolute path to the UE project directory.
                         Auto-detected when omitted.
        """
        path = _find_latest_log(project_dir)
        if not path:
            return make_error("No UE log file found. Provide 'project_dir' or run from project root.")
        size = os.path.getsize(path)
        mtime = os.path.getmtime(path)
        return {
            "success": True,
            "log_path": path,
            "size_bytes": size,
            "modified_time": mtime,
        }

    @mcp.tool()
    def read_log_tail(
        ctx: Context,
        log_path: Optional[str] = None,
        lines: int = 200,
        project_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Read the last N lines from a UE log file.

        Args:
            log_path: Absolute path to the log file. Auto-detected when omitted.
            lines: Number of tail lines to return (default 200).
            project_dir: Optional project directory for auto-detection.
        """
        if not log_path:
            log_path = _find_latest_log(project_dir)
        if not log_path or not os.path.isfile(log_path):
            return make_error(f"Log file not found: {log_path}")

        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                all_lines = f.readlines()
            tail = all_lines[-lines:]
            return {
                "success": True,
                "log_path": log_path,
                "total_lines": len(all_lines),
                "returned_lines": len(tail),
                "content": "".join(tail),
            }
        except Exception as e:
            return make_error(f"Failed to read log: {e}")

    @mcp.tool()
    def analyze_log_errors(
        ctx: Context,
        log_path: Optional[str] = None,
        max_errors: int = 50,
        project_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Extract errors and warnings from a UE log file.

        Args:
            log_path: Absolute path to the log file. Auto-detected when omitted.
            max_errors: Maximum number of error/warning lines to return.
            project_dir: Optional project directory for auto-detection.
        """
        if not log_path:
            log_path = _find_latest_log(project_dir)
        if not log_path or not os.path.isfile(log_path):
            return make_error(f"Log file not found: {log_path}")

        errors: List[Dict[str, Any]] = []
        warnings: List[Dict[str, Any]] = []

        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                for lineno, line in enumerate(f, 1):
                    line_strip = line.rstrip()
                    is_error = any(k in line_strip for k in _ERROR_KEYWORDS)
                    is_warning = not is_error and any(k in line_strip for k in _WARNING_KEYWORDS)

                    if is_error and len(errors) < max_errors:
                        m = _LOG_RE.match(line_strip)
                        errors.append({
                            "line": lineno,
                            "text": line_strip,
                            "category": m.group("category").strip() if m else "",
                            "message": m.group("message").strip() if m else line_strip,
                        })
                    elif is_warning and len(warnings) < max_errors:
                        m = _LOG_RE.match(line_strip)
                        warnings.append({
                            "line": lineno,
                            "text": line_strip,
                            "category": m.group("category").strip() if m else "",
                            "message": m.group("message").strip() if m else line_strip,
                        })

            return {
                "success": True,
                "log_path": log_path,
                "error_count": len(errors),
                "warning_count": len(warnings),
                "errors": errors,
                "warnings": warnings,
            }
        except Exception as e:
            return make_error(f"Failed to analyze log: {e}")

    @mcp.tool()
    def search_log(
        ctx: Context,
        pattern: str,
        log_path: Optional[str] = None,
        max_results: int = 100,
        project_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Search a UE log file for lines matching a regex pattern.

        Args:
            pattern: Regular expression to search for.
            log_path: Absolute path to the log file. Auto-detected when omitted.
            max_results: Maximum number of matching lines to return.
            project_dir: Optional project directory for auto-detection.
        """
        if not log_path:
            log_path = _find_latest_log(project_dir)
        if not log_path or not os.path.isfile(log_path):
            return make_error(f"Log file not found: {log_path}")

        try:
            compiled = re.compile(pattern, re.IGNORECASE)
        except re.error as e:
            return make_error(f"Invalid regex pattern: {e}")

        matches: List[Dict[str, Any]] = []
        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                for lineno, line in enumerate(f, 1):
                    if compiled.search(line) and len(matches) < max_results:
                        matches.append({"line": lineno, "text": line.rstrip()})

            return {
                "success": True,
                "log_path": log_path,
                "pattern": pattern,
                "match_count": len(matches),
                "matches": matches,
            }
        except Exception as e:
            return make_error(f"Failed to search log: {e}")

    @mcp.tool()
    def get_log_summary(
        ctx: Context,
        log_path: Optional[str] = None,
        project_dir: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Return a compact summary of a UE log file (category counts, last fatal error).

        Args:
            log_path: Absolute path to the log file. Auto-detected when omitted.
            project_dir: Optional project directory for auto-detection.
        """
        if not log_path:
            log_path = _find_latest_log(project_dir)
        if not log_path or not os.path.isfile(log_path):
            return make_error(f"Log file not found: {log_path}")

        total_lines = 0
        error_count = 0
        warning_count = 0
        last_fatal: Optional[str] = None
        category_counts: Dict[str, int] = {}

        try:
            with open(log_path, "r", encoding="utf-8", errors="replace") as f:
                for line in f:
                    total_lines += 1
                    line_s = line.rstrip()
                    m = _LOG_RE.match(line_s)
                    if m:
                        cat = m.group("category").strip()
                        category_counts[cat] = category_counts.get(cat, 0) + 1

                    if any(k in line_s for k in ["Fatal:", "fatal:", "Assertion failed"]):
                        last_fatal = line_s
                    if any(k in line_s for k in _ERROR_KEYWORDS):
                        error_count += 1
                    elif any(k in line_s for k in _WARNING_KEYWORDS):
                        warning_count += 1

            # Top 10 most active categories
            top_categories = sorted(category_counts.items(), key=lambda x: -x[1])[:10]

            return {
                "success": True,
                "log_path": log_path,
                "total_lines": total_lines,
                "error_count": error_count,
                "warning_count": warning_count,
                "last_fatal": last_fatal,
                "top_categories": [{"category": c, "count": n} for c, n in top_categories],
            }
        except Exception as e:
            return make_error(f"Failed to summarize log: {e}")

    logger.info("Log tools registered successfully")
