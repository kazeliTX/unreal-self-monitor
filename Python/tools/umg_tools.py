"""
UMG Tools for Unreal MCP.

This module provides tools for creating and manipulating UMG Widget Blueprints in Unreal Engine.

Parameter naming convention (mirrors the C++ API):
  - widget_name  (first arg)  → the Widget Blueprint asset name (sent as "blueprint_name" to C++)
  - The element name within the widget (text block, button, etc.) → sent as "widget_name" to C++
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context
from tools.base import send_unreal_command

logger = logging.getLogger("UnrealMCP")


def register_umg_tools(mcp: FastMCP):
    """Register UMG tools with the MCP server."""

    @mcp.tool()
    def create_umg_widget_blueprint(
        ctx: Context,
        widget_name: str,
        parent_class: str = "UserWidget",
        path: str = None,
    ) -> Dict[str, Any]:
        """Create a new UMG Widget Blueprint.

        Args:
            widget_name: Name of the widget blueprint to create
            parent_class: Parent class for the widget (default: UserWidget)
            path: Optional save directory (e.g. '/Game/Widgets/').
                  Defaults to '/Game/Widgets/' when omitted.
        """
        params = {"name": widget_name, "parent_class": parent_class}
        if path:
            params["path"] = path
        return send_unreal_command("create_umg_widget_blueprint", params)

    @mcp.tool()
    def add_text_block_to_widget(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        path: str = None,
    ) -> Dict[str, Any]:
        """Add a Text Block widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name to give the new Text Block element
            text: Initial text content
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the text block
            font_size: Font size in points
            color: [R, G, B, A] color values (0.0 to 1.0)
            path: Optional directory where the Widget Blueprint is located.
                  Defaults to '/Game/Widgets/' when omitted.
        """
        params = {
            "blueprint_name": widget_name,
            "widget_name": text_block_name,
            "text": text,
            "position": position,
            "size": size,
            "font_size": font_size,
            "color": color,
        }
        if path:
            params["path"] = path
        return send_unreal_command("add_text_block_to_widget", params)

    @mcp.tool()
    def add_button_to_widget(
        ctx: Context,
        widget_name: str,
        button_name: str,
        text: str = "",
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 50.0],
        font_size: int = 12,
        color: List[float] = [1.0, 1.0, 1.0, 1.0],
        background_color: List[float] = [0.1, 0.1, 0.1, 1.0],
        path: str = None,
    ) -> Dict[str, Any]:
        """Add a Button widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint
            button_name: Name to give the new Button element
            text: Text to display on the button
            position: [X, Y] position in the canvas panel
            size: [Width, Height] of the button
            font_size: Font size for button text
            color: [R, G, B, A] text color values (0.0 to 1.0)
            background_color: [R, G, B, A] button background color values (0.0 to 1.0)
            path: Optional directory where the Widget Blueprint is located.
        """
        params = {
            "blueprint_name": widget_name,
            "widget_name": button_name,
            "text": text,
            "position": position,
            "size": size,
            "font_size": font_size,
            "color": color,
            "background_color": background_color,
        }
        if path:
            params["path"] = path
        return send_unreal_command("add_button_to_widget", params)

    @mcp.tool()
    def bind_widget_event(
        ctx: Context,
        widget_name: str,
        widget_component_name: str,
        event_name: str,
        function_name: str = "",
        path: str = None,
    ) -> Dict[str, Any]:
        """Bind an event on a widget component to a function.

        Args:
            widget_name: Name of the target Widget Blueprint
            widget_component_name: Name of the widget component (button, etc.)
            event_name: Name of the event to bind (OnClicked, etc.)
            function_name: Name of the function to create/bind to
                           (defaults to '{widget_component_name}_{event_name}')
            path: Optional directory where the Widget Blueprint is located.
        """
        if not function_name:
            function_name = f"{widget_component_name}_{event_name}"
        params = {
            "blueprint_name": widget_name,
            "widget_name": widget_component_name,
            "event_name": event_name,
            "function_name": function_name,
        }
        if path:
            params["path"] = path
        return send_unreal_command("bind_widget_event", params)

    @mcp.tool()
    def add_widget_to_viewport(
        ctx: Context,
        widget_name: str,
        z_order: int = 0,
        path: str = None,
    ) -> Dict[str, Any]:
        """Add a Widget Blueprint instance to the viewport.

        Args:
            widget_name: Name of the Widget Blueprint to add
            z_order: Z-order for the widget (higher numbers appear on top)
            path: Optional directory where the Widget Blueprint is located.
        """
        params = {"blueprint_name": widget_name, "z_order": z_order}
        if path:
            params["path"] = path
        return send_unreal_command("add_widget_to_viewport", params)

    @mcp.tool()
    def set_text_block_binding(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        binding_property: str,
        binding_type: str = "Text",
        path: str = None,
    ) -> Dict[str, Any]:
        """Set up a property binding for a Text Block widget.

        Args:
            widget_name: Name of the target Widget Blueprint
            text_block_name: Name of the Text Block element to bind
            binding_property: Name of the property to bind to
            binding_type: Type of binding (Text, Visibility, etc.)
            path: Optional directory where the Widget Blueprint is located.
        """
        params = {
            "blueprint_name": widget_name,
            "widget_name": text_block_name,
            "binding_name": binding_property,
            "binding_type": binding_type,
        }
        if path:
            params["path"] = path
        return send_unreal_command("set_text_block_binding", params)

    @mcp.tool()
    def add_image_to_widget(
        ctx: Context,
        widget_name: str,
        image_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [100.0, 100.0],
        texture: Optional[str] = None,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Add an Image widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint.
            image_name: Name to give the new Image element.
            position: [X, Y] position in the canvas panel.
            size: [Width, Height] of the image.
            texture: Optional full asset path to a Texture2D.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": image_name,
            "position": position,
            "size": size,
        }
        if texture:
            params["texture"] = texture
        if path:
            params["path"] = path
        return send_unreal_command("add_image_to_widget", params)

    @mcp.tool()
    def add_progress_bar_to_widget(
        ctx: Context,
        widget_name: str,
        bar_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [200.0, 20.0],
        percent: float = 0.0,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Add a Progress Bar widget to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint.
            bar_name: Name to give the new Progress Bar element.
            position: [X, Y] position in the canvas panel.
            size: [Width, Height] of the bar.
            percent: Initial fill amount (0.0 – 1.0).
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": bar_name,
            "position": position,
            "size": size,
            "percent": percent,
        }
        if path:
            params["path"] = path
        return send_unreal_command("add_progress_bar_to_widget", params)

    @mcp.tool()
    def add_horizontal_box_to_widget(
        ctx: Context,
        widget_name: str,
        box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [400.0, 100.0],
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Add a Horizontal Box container to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint.
            box_name: Name to give the new Horizontal Box.
            position: [X, Y] position in the canvas panel.
            size: [Width, Height] of the box.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": box_name,
            "position": position,
            "size": size,
        }
        if path:
            params["path"] = path
        return send_unreal_command("add_horizontal_box_to_widget", params)

    @mcp.tool()
    def add_vertical_box_to_widget(
        ctx: Context,
        widget_name: str,
        box_name: str,
        position: List[float] = [0.0, 0.0],
        size: List[float] = [100.0, 400.0],
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Add a Vertical Box container to a UMG Widget Blueprint.

        Args:
            widget_name: Name of the target Widget Blueprint.
            box_name: Name to give the new Vertical Box.
            position: [X, Y] position in the canvas panel.
            size: [Width, Height] of the box.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": box_name,
            "position": position,
            "size": size,
        }
        if path:
            params["path"] = path
        return send_unreal_command("add_vertical_box_to_widget", params)

    @mcp.tool()
    def set_widget_visibility(
        ctx: Context,
        widget_name: str,
        element_name: str,
        visibility: str = "Visible",
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Set the visibility of a widget element.

        Args:
            widget_name: Name of the target Widget Blueprint.
            element_name: Name of the widget element to change.
            visibility: One of 'Visible', 'Hidden', 'Collapsed',
                        'HitTestInvisible', 'SelfHitTestInvisible'.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": element_name,
            "visibility": visibility,
        }
        if path:
            params["path"] = path
        return send_unreal_command("set_widget_visibility", params)

    @mcp.tool()
    def set_widget_anchor(
        ctx: Context,
        widget_name: str,
        element_name: str,
        min_x: float = 0.0,
        min_y: float = 0.0,
        max_x: float = 0.0,
        max_y: float = 0.0,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Set the anchor of a canvas-panel widget element.

        Args:
            widget_name: Name of the target Widget Blueprint.
            element_name: Name of the widget element.
            min_x, min_y: Top-left anchor point (0.0 – 1.0).
            max_x, max_y: Bottom-right anchor point (0.0 – 1.0).
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": element_name,
            "min_x": min_x,
            "min_y": min_y,
            "max_x": max_x,
            "max_y": max_y,
        }
        if path:
            params["path"] = path
        return send_unreal_command("set_widget_anchor", params)

    @mcp.tool()
    def update_text_block_text(
        ctx: Context,
        widget_name: str,
        text_block_name: str,
        text: str,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Update the text of an existing Text Block element.

        Args:
            widget_name: Name of the target Widget Blueprint.
            text_block_name: Name of the Text Block element to update.
            text: New text content.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {
            "blueprint_name": widget_name,
            "widget_name": text_block_name,
            "text": text,
        }
        if path:
            params["path"] = path
        return send_unreal_command("update_text_block_text", params)

    @mcp.tool()
    def get_widget_tree(
        ctx: Context,
        widget_name: str,
        path: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Get the widget hierarchy of a UMG Widget Blueprint as JSON.

        Args:
            widget_name: Name of the target Widget Blueprint.
            path: Optional directory where the Widget Blueprint is located.
        """
        params: Dict[str, Any] = {"blueprint_name": widget_name}
        if path:
            params["path"] = path
        return send_unreal_command("get_widget_tree", params)

    logger.info("UMG tools registered successfully")
