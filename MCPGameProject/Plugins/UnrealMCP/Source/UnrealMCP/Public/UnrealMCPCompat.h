#pragma once

/**
 * UnrealMCPCompat.h
 * Cross-version compatibility macros for UE4 (4.22+) and UE5.
 * Include this file in any .cpp that uses version-specific editor APIs.
 */

// ---------------------------------------------------------------------------
// Editor style: FAppStyle (UE5) vs FEditorStyle (UE4)
// ---------------------------------------------------------------------------
#if ENGINE_MAJOR_VERSION >= 5
	#include "Styling/AppStyle.h"
	#define MCP_STYLE_NAME FAppStyle::GetAppStyleSetName()
#else
	#include "EditorStyleSet.h"
	#define MCP_STYLE_NAME FEditorStyle::GetStyleSetName()
#endif

// ---------------------------------------------------------------------------
// Enhanced Input plugin availability
// UInputAction / UInputMappingContext require EnhancedInput, which ships with
// UE5 as a first-class module. On UE4 these classes do not exist.
// ---------------------------------------------------------------------------
#if ENGINE_MAJOR_VERSION >= 5
	#define MCP_ENHANCED_INPUT_SUPPORTED 1
#else
	#define MCP_ENHANCED_INPUT_SUPPORTED 0
#endif
