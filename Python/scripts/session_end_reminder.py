#!/usr/bin/env python3
"""
Stop hook: injects a documentation + git wrap-up reminder as a systemMessage to Claude.
Claude Code passes hook data as JSON via stdin; responses must be JSON on stdout.
"""
import json
import sys


def main():
    # Parse hook input
    try:
        hook_input = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    # REQUIRED: prevent infinite loop — if hook already fired once, let Claude stop
    if hook_input.get("stop_hook_active", False):
        sys.exit(0)

    response = {
        "decision": "block",
        "reason": (
            "Session wrap-up check: before stopping, please verify:\n"
            "1. Were any errors encountered? If yes → run /ue-note to save to References/Notes/\n"
            "2. Were plan steps completed? If yes → update checkboxes in References/Plans/\n"
            "3. Does CLAUDE.md need updating? If yes → update implementation progress\n"
            "4. Is Git enabled for this session? If yes → stage relevant files and create a git commit\n"
            "   Branch: check current branch with `git branch --show-current`\n"
            "   Commit format: feat/fix/docs: [summary] + Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>\n"
            "If nothing needs documenting or committing, you may stop."
        ),
    }
    print(json.dumps(response))
    sys.exit(0)


if __name__ == "__main__":
    main()
