"""
Blueprint Node Tools for Unreal MCP.

This module provides tools for manipulating Blueprint graph nodes and connections.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command

logger = logging.getLogger("UnrealMCP")


def register_blueprint_node_tools(mcp: FastMCP):
    """Register Blueprint node manipulation tools with the MCP server."""

    @mcp.tool()
    def add_blueprint_event_node(
        ctx: Context,
        blueprint_name: str,
        event_name: str,
        node_position=None
    ) -> Dict[str, Any]:
        """Add an event node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            event_name: Name of the event (e.g. 'ReceiveBeginPlay', 'ReceiveTick')
            node_position: Optional [X, Y] position in the graph
        """
        return send_unreal_command("add_blueprint_event_node", {
            "blueprint_name": blueprint_name,
            "event_name": event_name,
            "node_position": node_position or [0, 0],
        })

    @mcp.tool()
    def add_blueprint_input_action_node(
        ctx: Context,
        blueprint_name: str,
        action_name: str,
        node_position=None
    ) -> Dict[str, Any]:
        """Add an input action event node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            action_name: Name of the input action to respond to
            node_position: Optional [X, Y] position in the graph
        """
        return send_unreal_command("add_blueprint_input_action_node", {
            "blueprint_name": blueprint_name,
            "action_name": action_name,
            "node_position": node_position or [0, 0],
        })

    @mcp.tool()
    def add_blueprint_function_node(
        ctx: Context,
        blueprint_name: str,
        target: str,
        function_name: str,
        params=None,
        node_position=None
    ) -> Dict[str, Any]:
        """Add a function call node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            target: Target object for the function (component name or self)
            function_name: Name of the function to call
            params: Optional parameters to set on the function node
            node_position: Optional [X, Y] position in the graph
        """
        return send_unreal_command("add_blueprint_function_node", {
            "blueprint_name": blueprint_name,
            "target": target,
            "function_name": function_name,
            "params": params or {},
            "node_position": node_position or [0, 0],
        })

    @mcp.tool()
    def connect_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        source_node_id: str,
        source_pin: str,
        target_node_id: str,
        target_pin: str
    ) -> Dict[str, Any]:
        """Connect two nodes in a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            source_node_id: ID of the source node
            source_pin: Name of the output pin on the source node
            target_node_id: ID of the target node
            target_pin: Name of the input pin on the target node
        """
        return send_unreal_command("connect_blueprint_nodes", {
            "blueprint_name": blueprint_name,
            "source_node_id": source_node_id,
            "source_pin": source_pin,
            "target_node_id": target_node_id,
            "target_pin": target_pin,
        })

    @mcp.tool()
    def add_blueprint_variable(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        variable_type: str,
        is_exposed: bool = False
    ) -> Dict[str, Any]:
        """Add a variable to a Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            variable_name: Name of the variable
            variable_type: Type of the variable (Boolean, Integer, Float, Vector, etc.)
            is_exposed: Whether to expose the variable to the editor
        """
        return send_unreal_command("add_blueprint_variable", {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
            "variable_type": variable_type,
            "is_exposed": is_exposed,
        })

    @mcp.tool()
    def add_blueprint_get_self_component_reference(
        ctx: Context,
        blueprint_name: str,
        component_name: str,
        node_position=None
    ) -> Dict[str, Any]:
        """Add a node that gets a reference to a component owned by the current Blueprint.

        Args:
            blueprint_name: Name of the target Blueprint
            component_name: Name of the component to get a reference to
            node_position: Optional [X, Y] position in the graph
        """
        return send_unreal_command("add_blueprint_get_self_component_reference", {
            "blueprint_name": blueprint_name,
            "component_name": component_name,
            "node_position": node_position or [0, 0],
        })

    @mcp.tool()
    def add_blueprint_self_reference(
        ctx: Context,
        blueprint_name: str,
        node_position=None
    ) -> Dict[str, Any]:
        """Add a 'Get Self' node to a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            node_position: Optional [X, Y] position in the graph
        """
        return send_unreal_command("add_blueprint_self_reference", {
            "blueprint_name": blueprint_name,
            "node_position": node_position or [0, 0],
        })

    @mcp.tool()
    def find_blueprint_nodes(
        ctx: Context,
        blueprint_name: str,
        node_type=None,
        event_type=None
    ) -> Dict[str, Any]:
        """Find nodes in a Blueprint's event graph.

        Args:
            blueprint_name: Name of the target Blueprint
            node_type: Optional type of node to find (Event, Function, Variable, etc.)
            event_type: Optional specific event type to find (BeginPlay, Tick, etc.)
        """
        return send_unreal_command("find_blueprint_nodes", {
            "blueprint_name": blueprint_name,
            "node_type": node_type,
            "event_type": event_type,
        })

    # ------------------------------------------------------------------
    # New node commands
    # ------------------------------------------------------------------

    @mcp.tool()
    def add_blueprint_get_variable_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a 'Get Variable' node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            variable_name: Name of the variable to get.
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_get_variable_node", params)

    @mcp.tool()
    def add_blueprint_set_variable_node(
        ctx: Context,
        blueprint_name: str,
        variable_name: str,
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a 'Set Variable' node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            variable_name: Name of the variable to set.
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "variable_name": variable_name,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_set_variable_node", params)

    @mcp.tool()
    def add_blueprint_branch_node(
        ctx: Context,
        blueprint_name: str,
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a Branch (if/else) node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {"blueprint_name": blueprint_name}
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_branch_node", params)

    @mcp.tool()
    def add_blueprint_sequence_node(
        ctx: Context,
        blueprint_name: str,
        output_count: int = 2,
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a Sequence node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            output_count: Number of sequence outputs (2–8).
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "output_count": output_count,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_sequence_node", params)

    @mcp.tool()
    def add_blueprint_cast_node(
        ctx: Context,
        blueprint_name: str,
        target_class: str,
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a Cast node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            target_class: Class to cast to (e.g. 'MyCharacter').
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "target_class": target_class,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_cast_node", params)

    @mcp.tool()
    def add_blueprint_math_node(
        ctx: Context,
        blueprint_name: str,
        operation: str,
        type: str = "Float",
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a Math operation node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            operation: Operation name ('Add', 'Subtract', 'Multiply', 'Divide',
                       'Clamp', 'Abs', 'Min', 'Max', etc.).
            type: Value type ('Float', 'Int', 'Vector').
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "operation": operation,
            "type": type,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_math_node", params)

    @mcp.tool()
    def add_blueprint_print_string_node(
        ctx: Context,
        blueprint_name: str,
        message: str = "Hello",
        graph_name: Optional[str] = None,
        node_position: Optional[List[float]] = None,
    ) -> Dict[str, Any]:
        """Add a Print String node to a Blueprint graph.

        Args:
            blueprint_name: Name of the Blueprint.
            message: Default message text.
            graph_name: Target graph name. Defaults to EventGraph.
            node_position: Optional [X, Y] position in the graph.
        """
        params: Dict[str, Any] = {
            "blueprint_name": blueprint_name,
            "message": message,
        }
        if graph_name:
            params["graph_name"] = graph_name
        if node_position:
            params["node_position"] = node_position
        return send_unreal_command("add_blueprint_print_string_node", params)

    @mcp.tool()
    def add_blueprint_custom_function(
        ctx: Context,
        blueprint_name: str,
        function_name: str,
    ) -> Dict[str, Any]:
        """Create a new custom function graph in a Blueprint.

        Args:
            blueprint_name: Name of the Blueprint.
            function_name: Name for the new function.
        """
        return send_unreal_command("add_blueprint_custom_function", {
            "blueprint_name": blueprint_name,
            "function_name": function_name,
        })

    logger.info("Blueprint node tools registered successfully")
