/*

---------- OpenGL Surface object ----------
	Replacement for Min's OpenGL Base object using cSurface API, authored by Nassic (https://github.com/nasiratif/OpenGL-Surface-object)

*/

#include <Surface.h>
#include <Windows.h>

#include <new>

#include <cstdlib>
#include <cstdint>
#include <gl/GL.h>

#include <algorithm>

#define IDENTIFIER	MAKEID(O,G,L,S)

// ------------------------------
// DEFINITION OF CONDITIONS CODES
// ------------------------------
#define	CND_ONCREATES 0
#define	CND_ONCREATEF 1

#define	CND_ONREDRAW 2

#define	CND_ISAUTOREDRAW 3

#define	CND_ISTRANSP 4

#define	CND_LAST 5

// ---------------------------
// DEFINITION OF ACTIONS CODES
// ---------------------------
#define	ACT_SETSIZE 0

#define	ACT_CREATESURF 1
#define	ACT_MAKECUR 2

#define	ACT_AUTOREDRAWE 3
#define	ACT_AUTOREDRAWD 4
#define	ACT_REDRAW 5

#define	ACT_TRANSPE 6
#define	ACT_TRANSPD 7
#define	ACT_SETTRANSP 8

#define	ACT_LAST 9

// -------------------------------
// DEFINITION OF EXPRESSIONS CODES
// -------------------------------
#define	EXP_WIDTH 0
#define	EXP_HEIGHT 1
#define	EXP_TRANSPCOLOR 2
#define	EXP_LAST 3

// ---------------------
// OBJECT DATA STRUCTURE 
// ---------------------
// Used at edit time and saved in the MFA/CCN/EXE files

typedef struct tagEDATA_V1
{
	// Header - required
	extHeader		eHeader;

	// Object's data
	bool autoRedraw;
	COLORREF transpColor;

	short			swidth;
	short			sheight;

} EDITDATA;
typedef EDITDATA *			LPEDATA;

// Object versions
#define	KCX_CURRENT_VERSION			1

// --------------------------------
// RUNNING OBJECT DATA STRUCTURE
// --------------------------------
// Used at runtime. Initialize it in the CreateRunObject function.
// Free any allocated memory or object in the DestroyRunObject function.
//
// Note: if you store C++ objects in this structure and do not store 
// them as pointers, you must call yourself their constructor in the
// CreateRunObject function and their destructor in the DestroyRunObject
// function. As the RUNDATA structure is a simple C structure and not a C++ object.

typedef struct tagRDATA
{
	// Main header - required
	headerObject	rHo;					// Header

	// Optional headers - depend on the OEFLAGS value, see documentation and examples for more info
	rCom			rc;				// Common structure for movements & animations
	rMvt			rm;				// Movements
	rSpr			rs;				// Sprite (displayable objects)
	rVal			rv;				// Alterable values

	// Object's runtime data
	bool autoRedraw;
	COLORREF transpColor;

	LPSURFACE surf;
	LPSMASK mask;

	HWND hWnd;
	HDC hdc;

	HGLRC glContext;

	int bufferSize;
	int alignedPitch;
	LPBYTE srcBits;
} RUNDATA;
typedef	RUNDATA	*			LPRDATA;

// Size when editing the object under level editor
// -----------------------------------------------
#define	MAX_EDITSIZE			sizeof(EDITDATA)

// Default flags - see documentation for more info
// -------------
#define	OEFLAGS      			OEFLAG_SPRITES | OEFLAG_BACKSAVE | OEFLAG_MOVEMENTS | OEFLAG_VALUES
#define	OEPREFS      			OEPREFS_INKEFFECTS | OEPREFS_BACKSAVE | OEPREFS_BACKEFFECTS | OEPREFS_SCROLLINGINDEPENDANT | OEPREFS_FINECOLLISIONS


// If to handle message, specify the priority of the handling procedure
// 0= low, 255= very high. You should use 100 as normal.                                                
// --------------------------------------------------------------------
#define	WINDOWPROC_PRIORITY		100

// FUNCTIONS:
// -----
bool CreateGLSurface(LPRDATA rdPtr);
bool ResetWindow(LPRDATA rdPtr);
void UpdateSurface(LPRDATA rdPtr);
// Only necessary due to Standard display mode
void UpdateZone(LPRDATA rdPtr);

// Align an integer to GL_PACK_ALIGNMENT
// Algorithm from Stack Overflow, see definition for license info
int AlignValue(int a);
// -----