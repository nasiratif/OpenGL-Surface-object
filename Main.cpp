// ============================================================================
//
// This file are where the Conditions/Actions/Expressions are defined.
// You can manually enter these, or use CICK (recommended)
// See the Extension FAQ in this SDK for more info and where to download it
//
// ============================================================================

// Common Include
#include	"common.h"

// Quick memo: content of the eventInformations arrays
// ---------------------------------------------------
// Menu ID
// String ID
// Code
// Flags
// Number_of_parameters
// Parameter_type [Number_of_parameters]
// Parameter_TitleString [Number_of_parameters]

// Definitions of parameters for each condition
short conditionsInfos[] =
{
	IDMN_ONCREATES, IDSC_ONCREATES, CND_ONCREATES, 0, 
	0,

	IDMN_ONCREATEF, IDSC_ONCREATEF, CND_ONCREATEF, 0,
	0,


	IDMN_ONREDRAW, IDSC_ONREDRAW, CND_ONREDRAW, 0,
	0,
};

// Definitions of parameters for each action
short actionsInfos[] =
{
	IDMN_SETSIZE, IDSA_SETWIDTH, ACT_SETSIZE, 0, 
	2, 
		PARAM_EXPRESSION, PARAM_EXPRESSION, 
		IDSA_SETSIZE1, IDSA_SETSIZE2,


	IDMN_CREATESURF, IDSA_CREATESURF, ACT_CREATESURF, 0,
	0,

	IDMN_MAKECUR, IDSA_MAKECUR,	ACT_MAKECUR, 0,
	0,

	IDMN_AUTOREDRAWE, IDSA_AUTOREDRAWE,	ACT_AUTOREDRAWE, 0,
	0,
	IDMN_AUTOREDRAWD, IDSA_AUTOREDRAWD,	ACT_AUTOREDRAWD, 0,
	0,
	IDMN_REDRAW, IDSA_REDRAW, ACT_REDRAW, 0,
	0,

	IDMN_SETTRANSP, IDSA_SETTRANSP, ACT_SETTRANSP, 0,
	1,
		PARAM_COLOUR,
		IDSA_SETTRANSP1
};

// Definitions of parameters for each expression
short expressionsInfos[] =
{
	IDMN_WIDTH, IDSE_WIDTH, EXP_WIDTH, 0,
	0,

	IDMN_HEIGHT, IDSE_HEIGHT, EXP_HEIGHT, 0,
	0,


	IDMN_TRANSPCOLOR, IDSE_TRANSPCOLOR, EXP_TRANSPCOLOR, 0,
	0,
};



// ============================================================================
//
// CONDITION ROUTINES
// 
// ============================================================================

// -----------------
// Sample Condition
// -----------------
// Returns TRUE when the two values are equal!
// 

long WINAPI DLLExport Immediate(LPRDATA rdPtr, long param1, long param2)
{
	return TRUE;
}


// ============================================================================
//
// ACTIONS ROUTINES
// 
// ============================================================================

// -----------------
// Sample Action
// -----------------
// Does nothing!
// 

short WINAPI DLLExport SetSize(LPRDATA rdPtr, long param1, long param2)
{
	auto differentWidth = false;
	auto differentHeight = false;
	if (param1)
	{
		param1 = std::clamp<long>(param1, 1, 32767);
		differentWidth = rdPtr->rHo.hoImgWidth != param1;
		rdPtr->rHo.hoImgWidth = param1;
	}
	if (param2)
	{
		param2 = std::clamp<long>(param2, 1, 32767);
		differentHeight = rdPtr->rHo.hoImgHeight != param2;
		rdPtr->rHo.hoImgHeight = param2;
	}

	if (rdPtr->surf && (differentWidth || differentHeight))
	{
		CreateGLSurface(rdPtr);
	}

	return 0;
}

short WINAPI DLLExport CreateGLWindow(LPRDATA rdPtr, long param1, long param2)
{
	if (CreateGLSurface(rdPtr))
		callRunTimeFunction(rdPtr, RFUNCTION_GENERATEEVENT, CND_ONCREATES, NULL);
	else
		callRunTimeFunction(rdPtr, RFUNCTION_GENERATEEVENT, CND_ONCREATEF, NULL);
	return 0;
}

short WINAPI DLLExport MakeCurrent(LPRDATA rdPtr, long param1, long param2)
{
	if (rdPtr->hdc && rdPtr->glContext)
	{
		wglMakeCurrent(rdPtr->hdc, rdPtr->glContext);
		return 0;
	}
	return 0;
}


short WINAPI DLLExport AutoRedrawEnable(LPRDATA rdPtr, long param1, long param2)
{
	rdPtr->autoRedraw = true;
	return 0;
}

short WINAPI DLLExport AutoRedrawDisable(LPRDATA rdPtr, long param1, long param2)
{
	rdPtr->autoRedraw = false;
	return 0;
}

short WINAPI DLLExport Redraw(LPRDATA rdPtr, long param1, long param2)
{
	UpdateSurface(rdPtr);
	return 0;
}


short WINAPI DLLExport SetTransparentColor(LPRDATA rdPtr, long param1, long param2)
{
	rdPtr->transpColor = (COLORREF)param1;
	rdPtr->surf->SetTransparentColor(rdPtr->transpColor);
	return 0;
}


// ============================================================================
//
// EXPRESSIONS ROUTINES
// 
// ============================================================================

long WINAPI DLLExport Width(LPRDATA rdPtr,long param1)
{
	return rdPtr->rHo.hoImgWidth;
}

long WINAPI DLLExport Height(LPRDATA rdPtr, long param1)
{
	return rdPtr->rHo.hoImgHeight;
}


long WINAPI DLLExport TransparentColor(LPRDATA rdPtr, long param1)
{
	return rdPtr->transpColor;
}



// ----------------------------------------------------------
// Condition / Action / Expression jump table
// ----------------------------------------------------------
// Contains the address inside the extension of the different
// routines that handle the action, conditions and expressions.
// Located at the end of the source for convinience
// Must finish with a 0
//
long (WINAPI * ConditionJumps[])(LPRDATA rdPtr, long param1, long param2) = 
{ 
	Immediate,
	Immediate,
	Immediate,
	0
};
	
short (WINAPI * ActionJumps[])(LPRDATA rdPtr, long param1, long param2) =
{
	SetSize,
	CreateGLWindow,
	MakeCurrent,
	AutoRedrawEnable,
	AutoRedrawDisable,
	Redraw,
	SetTransparentColor,
	0
};

long (WINAPI * ExpressionJumps[])(LPRDATA rdPtr, long param) = 
{     
	Width,
	Height,
	TransparentColor,
	0
};