Check the current status of the Unreal Engine editor and MCP connection.

Run a comprehensive status check:
1. Use the MCP tool `check_editor_status` to get the 5-dimension readiness matrix (process/port/ping/WorldLoaded/overall)
2. Use `ping` to verify TCP connectivity
3. Use `get_capabilities` to list all registered commands and count them
4. Report:
   - Editor ready: yes/no
   - MCP connected: yes/no
   - Available command count
   - Any warnings or issues found

If the editor is not ready, suggest:
- Run `launch_editor` to start it
- Run `wait_for_editor_ready` to wait for it to initialize
