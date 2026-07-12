// ============================================================================
//
// This file contains routines that are handled during the Runtime.
//
// Including creating, display, and handling your object.
// 
// ============================================================================

// Common Include
#include	"common.h"

/*

---------- COMMON FUNCTIONS ----------

*/

bool CreateGLSurface(LPRDATA rdPtr, HGLRC newGLContext, HDC newDC)
{
	cSurface* proto;
	if (GetSurfacePrototype(&proto, 32, ST_MEMORY, SD_DIB))
	{
		if (newGLContext && newDC)
		{
			if (rdPtr->glContext != newGLContext)
			{
				rdPtr->ownsContext = false;
				rdPtr->glContext = newGLContext;
				rdPtr->hdc = newDC;
				rdPtr->hWnd = WindowFromDC(rdPtr->hdc);
			}
		}
		else if (!rdPtr->glContext)
		{
			rdPtr->ownsContext = true;
		}

		rdPtr->surf->Create(rdPtr->rHo.hoImgWidth, rdPtr->rHo.hoImgHeight, proto);
		// For transparency to work in Standard display mode we need to manage an alpha surface
		if (!rdPtr->hwa)
		{
			rdPtr->surf->CreateAlpha();
			auto alphaSurf = rdPtr->surf->GetAlphaSurface();
			if (alphaSurf)
				alphaSurf->Fill((COLORREF)0);
		}
		rdPtr->surf->SetTransparentColor(rdPtr->transpColor);
		rdPtr->surf->Fill(rdPtr->transpColor);
		rdPtr->alignedPitch = AlignValue(rdPtr->surf->GetWidth() * (rdPtr->surf->GetDepth() / CHAR_BIT));
		rdPtr->rc.rcChanged = TRUE;

		if (!ResetWindow(rdPtr))
			return false;

		if (!rdPtr->glContext)
		{
			PIXELFORMATDESCRIPTOR pfd;
			ZeroMemory(&pfd, sizeof(pfd));
			pfd.nSize = sizeof(pfd);
			pfd.nVersion = 1;
			pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.iLayerType = PFD_MAIN_PLANE;
			pfd.cColorBits = rdPtr->surf->GetDepth();
			pfd.cDepthBits = 24;
			pfd.cStencilBits = 8;

			auto fmt = ChoosePixelFormat(rdPtr->hdc, &pfd);
			if (!SetPixelFormat(rdPtr->hdc, fmt, &pfd))
				return false;

			rdPtr->glContext = wglCreateContext(rdPtr->hdc);
			if (rdPtr->glContext)
				wglMakeCurrent(rdPtr->hdc, rdPtr->glContext);
			else
				return false;
		}

		if (rdPtr->ownsContext)
			glViewport(0, 0, rdPtr->rHo.hoImgWidth, rdPtr->rHo.hoImgHeight);
	}
	else
	{
		return false;
	}

	return true;
}

bool ResetWindow(LPRDATA rdPtr)
{
	if (!rdPtr->ownsContext)
		return true;

	if (rdPtr->hWnd)
	{
		SetWindowPos(rdPtr->hWnd, NULL, 0, 0, rdPtr->rHo.hoImgWidth, rdPtr->rHo.hoImgHeight, SWP_NOREPOSITION);
	}
	else
	{
		static WNDCLASSEX wndClass = {};
		if (!wndClass.cbSize)
		{
			wndClass.cbSize = sizeof(wndClass);
			wndClass.hInstance = rdPtr->rHo.hoAdRunHeader->rh4.rh4Instance;
			wndClass.lpszClassName = _T("OpenGL-Surface-Ext");
			wndClass.lpfnWndProc = DefWindowProc;
			wndClass.style = CS_HREDRAW | CS_VREDRAW;
			RegisterClassEx(&wndClass);
		}

		// In order to get a DC that always matches a particular size I'm regrettably resorting to just creating an off-screen window with it's only purpose being to obtain it's DC, which will always match the window's dimensions
		// I'm not aware of any other possible ways to approach this; I tried CreateCompatibleBitmap then selecting that into the DC but that doesn't alter it's dimensions at all..
		rdPtr->hWnd = CreateWindow(
			wndClass.lpszClassName,
			_T("GL"),
			WS_CHILD,
			0, 0,
			rdPtr->rHo.hoImgWidth,
			rdPtr->rHo.hoImgHeight,
			rdPtr->rHo.hoAdRunHeader->rhHEditWin,
			NULL,
			wndClass.hInstance,
			NULL
		);

		rdPtr->hdc = GetDC(rdPtr->hWnd);
		return rdPtr->hWnd != nullptr;
	}

	return true;
}

void UpdateSurface(LPRDATA rdPtr)
{
	if (!rdPtr->glContext)
		return;

	// Do OpenGL rendering:
	callRunTimeFunction(rdPtr, RFUNCTION_GENERATEEVENT, CND_ONREDRAW, NULL);

	auto bits = rdPtr->surf->LockBuffer();
	if (bits)
	{
		auto pitch = rdPtr->surf->GetPitch();
		auto depth = rdPtr->surf->GetDepth();

		auto width = rdPtr->surf->GetWidth();
		auto height = rdPtr->surf->GetHeight();

		auto newSize = rdPtr->alignedPitch * height;
		if (!rdPtr->srcBits || rdPtr->bufferSize != newSize)
		{
			free(rdPtr->srcBits);
			rdPtr->srcBits = (BYTE*)malloc(newSize);
			rdPtr->bufferSize = newSize;
		}

		glFlush();
		glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, rdPtr->srcBits);

		auto dest = bits;
		auto destEnd = bits + pitch * height;
		auto destRowSize = width * (depth / CHAR_BIT);

		// For the source bitmap, we have to start from the bottom row then work our way towards the top due to OpenGL's bottom-left origin convention as opposed to Fusion's top-left origin
		auto src = rdPtr->srcBits + height * rdPtr->alignedPitch;
		while (dest < destEnd)
		{
			src -= rdPtr->alignedPitch;
			memcpy(dest, src, destRowSize);
			dest += pitch;
		}

		if (!rdPtr->hwa && (rdPtr->rs.rsEffect & EFFECTFLAG_TRANSPARENT))
		{
			auto alphaSurf = rdPtr->surf->GetAlphaSurface();
			if (alphaSurf)
			{
				// Reverse channels:
				auto transpColor = ((rdPtr->transpColor & 0xFF) << 16) | (rdPtr->transpColor & 0xFF00) | ((rdPtr->transpColor & 0xFF0000) >> 16);

				auto depthBytes = depth / CHAR_BIT;
				auto alphaBits = rdPtr->surf->LockAlpha();
				auto alphaPitch = rdPtr->surf->GetAlphaPitch();
				auto alphaDepth = alphaSurf->GetDepth();
				auto alphaDepthBytes = alphaDepth / CHAR_BIT;
				if (alphaBits)
				{
					auto dest = alphaBits;
					auto src = bits;

					for (int r = 0; r < height; ++r)
					{
						auto destX = dest;
						auto srcX = src;
						for (int p = 0; p < width; ++p)
						{
							auto color = *((uint32_t*)srcX) & 0xFFFFFF;
							auto opaque = color != transpColor ? 0xFF : 0x0;
							memset(destX, opaque, alphaDepthBytes);
							destX += alphaDepthBytes;
							srcX += depthBytes;
						}
						dest += alphaPitch;
						src += pitch;
					}
					rdPtr->surf->UnlockAlpha();
				}
			}
		}

		rdPtr->surf->UnlockBuffer(bits);
		UpdateZone(rdPtr);
	}
}

void UpdateZone(LPRDATA rdPtr)
{
	rdPtr->rc.rcChanged = TRUE;
	RECT rc;
	rc.left = rdPtr->rHo.hoX - rdPtr->rHo.hoAdRunHeader->rhWindowX;
	rc.top = rdPtr->rHo.hoY - rdPtr->rHo.hoAdRunHeader->rhWindowY;
	rc.right = rc.left + rdPtr->rHo.hoImgWidth;
	rc.bottom = rc.top + rdPtr->rHo.hoImgHeight;
	WinAddZone(rdPtr->rHo.hoAdRunHeader->rhIdEditWin, &rc);
}


int AlignValue(int a)
{
	// Source - https://stackoverflow.com/a/3407254
	// Posted by Mark Ransom, modified by community. See post 'Timeline' for change history
	// Retrieved 2026-06-27, License - CC BY-SA 3.0

	int multiple = 0;
	glGetIntegerv(GL_PACK_ALIGNMENT, &multiple);

	if (multiple == 0)
		return a;

	int remainder = a % multiple;
	if (remainder == 0)
		return a;

	return a + multiple - remainder;

}

// DEBUGGER /////////////////////////////////////////////////////////////////

#if !defined(RUN_ONLY)
// Identifiers of items displayed in the debugger
enum
{
// Example
// -------
//	DB_CURRENTSTRING,
//	DB_CURRENTVALUE,
//	DB_CURRENTCHECK,
//	DB_CURRENTCOMBO
};

// Items displayed in the debugger
WORD DebugTree[]=
{
// Example
// -------
//	DB_CURRENTSTRING|DB_EDITABLE,
//	DB_CURRENTVALUE|DB_EDITABLE,
//	DB_CURRENTCHECK,
//	DB_CURRENTCOMBO,

	// End of table (required)
	DB_END
};

#endif // !defined(RUN_ONLY)


// --------------------
// GetRunObjectDataSize
// --------------------
// Returns the size of the runtime datazone of the object
// 
ushort WINAPI DLLExport GetRunObjectDataSize(fprh rhPtr, LPEDATA edPtr)
{
	return(sizeof(RUNDATA));
}


// ---------------
// CreateRunObject
// ---------------
// The routine where the object is actually created
// 
short WINAPI DLLExport CreateRunObject(LPRDATA rdPtr, LPEDATA edPtr, fpcob cobPtr)
{
	rdPtr->autoRedraw = edPtr->autoRedraw;
	rdPtr->transpColor = edPtr->transpColor;

	rdPtr->rHo.hoX = cobPtr->cobX;
	rdPtr->rHo.hoY = cobPtr->cobY;
	rdPtr->rHo.hoImgWidth = edPtr->swidth;
	rdPtr->rHo.hoImgHeight = edPtr->sheight;

	rdPtr->surf = NewSurface();

	rdPtr->hWnd = NULL;
	rdPtr->hdc = NULL;

	rdPtr->glContext = NULL;
	rdPtr->ownsContext = true;

	auto surf = WinGetSurface((int)rdPtr->rHo.hoAdRunHeader->rhIdEditWin);
	rdPtr->hwa = surf ? surf->GetDriver() >= SD_D3D9 : false;

	rdPtr->bufferSize = 0;
	rdPtr->alignedPitch = 0;
	rdPtr->srcBits = nullptr;

	// No errors
	return 0;
}


// ----------------
// DestroyRunObject
// ----------------
// Destroys the run-time object
// 
short WINAPI DLLExport DestroyRunObject(LPRDATA rdPtr, long fast)
{
	if (rdPtr->ownsContext)
	{
		if (rdPtr->glContext)
			wglDeleteContext(rdPtr->glContext);
		if (rdPtr->hdc)
			ReleaseDC(rdPtr->hWnd, rdPtr->hdc);
	}
	if (rdPtr->hWnd)
		DestroyWindow(rdPtr->hWnd);

	free(rdPtr->mask);
	DeleteSurface(rdPtr->surf);

	free(rdPtr->srcBits);

	// No errors
	return 0;
}


// ----------------
// HandleRunObject
// ----------------
// Called (if you want) each loop, this routine makes the object live
// 
short WINAPI DLLExport HandleRunObject(LPRDATA rdPtr)
{
	if (rdPtr->glContext && rdPtr->autoRedraw)
		UpdateSurface(rdPtr);

	return 0;
}

// ----------------
// DisplayRunObject
// ----------------
// Draw the object in the application screen.
// 
short WINAPI DLLExport DisplayRunObject(LPRDATA rdPtr)
{
/*
   If you return REFLAG_DISPLAY in HandleRunObject this routine will run.
*/
	// Ok
	return 0;
}

// -------------------
// GetRunObjectSurface
// -------------------
// Implement this function instead of DisplayRunObject if your extension
// supports ink effects and transitions. Note: you can support ink effects
// in DisplayRunObject too, but this is automatically done if you implement
// GetRunObjectSurface (MMF applies the ink effect to the surface).
//
// Note: do not forget to enable the function in the .def file 
// if you remove the comments below.

cSurface* WINAPI DLLExport GetRunObjectSurface(LPRDATA rdPtr)
{
	return rdPtr->glContext ? rdPtr->surf : nullptr;
}


// -------------------------
// GetRunObjectCollisionMask
// -------------------------
// Implement this function if your extension supports fine collision mode (OEPREFS_FINECOLLISIONS),
// Or if it's a background object and you want Obstacle properties for this object.
//
// Should return NULL if the object is not transparent.
//
// Note: do not forget to enable the function in the .def file 
// if you remove the comments below.
//

LPSMASK WINAPI DLLExport GetRunObjectCollisionMask(LPRDATA rdPtr, LPARAM lParam)
{
	if (!(rdPtr->rs.rsEffect & EFFECTFLAG_TRANSPARENT))
		return NULL;

	// Transparent? Create mask
	auto pMask = rdPtr->mask;
	if (rdPtr->surf)
	{
		auto dwMaskSize = rdPtr->surf->CreateMask(NULL, lParam);
		if (dwMaskSize)
		{
			if (pMask)
				free(pMask);

			pMask = (LPSMASK)calloc(dwMaskSize, 1);
			if (pMask)
			{
				rdPtr->surf->CreateMask(pMask, lParam);
				rdPtr->mask = pMask;
			}
		}
	}

	return pMask;
}


// ----------------
// PauseRunObject
// ----------------
// Enters the pause mode
// 
short WINAPI DLLExport PauseRunObject(LPRDATA rdPtr)
{
	// Ok
	return 0;
}


// -----------------
// ContinueRunObject
// -----------------
// Quits the pause mode
//
short WINAPI DLLExport ContinueRunObject(LPRDATA rdPtr)
{
	// Ok
	return 0;
}

// -----------------
// SaveRunObject
// -----------------
// Saves the object to disk
// 

BOOL WINAPI SaveRunObject(LPRDATA rdPtr, HANDLE hf)
{            
	BOOL bOK = FALSE;

#ifndef VITALIZE

	// Save the object's data here

	// OK
	bOK = TRUE;

#endif // VITALIZE

	return bOK;
}

// -----------------
// LoadRunObject
// -----------------
// Loads the object from disk
// 
BOOL WINAPI LoadRunObject(LPRDATA rdPtr, HANDLE hf)
{            
	BOOL bOK=FALSE;

	// Load the object's data here

	// OK
	bOK = TRUE;

	return bOK; 
}




// ============================================================================
//
// START APP / END APP / START FRAME / END FRAME routines
// 
// ============================================================================

// -------------------
// StartApp
// -------------------
// Called when the application starts or restarts.
// Useful for storing global data
// 
void WINAPI DLLExport StartApp(mv _far *mV, CRunApp* pApp)
{
	// Example
	// -------
	// Delete global data (if restarts application)
//	CMyData* pData = (CMyData*)mV->mvGetExtUserData(pApp, hInstLib);
//	if ( pData != NULL )
//	{
//		delete pData;
//		mV->mvSetExtUserData(pApp, hInstLib, NULL);
//	}
}

// -------------------
// EndApp
// -------------------
// Called when the application ends.
// 
void WINAPI DLLExport EndApp(mv _far *mV, CRunApp* pApp)
{
	// Example
	// -------
	// Delete global data
//	CMyData* pData = (CMyData*)mV->mvGetExtUserData(pApp, hInstLib);
//	if ( pData != NULL )
//	{
//		delete pData;
//		mV->mvSetExtUserData(pApp, hInstLib, NULL);
//	}
}

// -------------------
// StartFrame
// -------------------
// Called when the frame starts or restarts.
// 
void WINAPI DLLExport StartFrame(mv _far *mV, DWORD dwReserved, int nFrameIndex)
{
}

// -------------------
// EndFrame
// -------------------
// Called when the frame ends.
// 
void WINAPI DLLExport EndFrame(mv _far *mV, DWORD dwReserved, int nFrameIndex)
{
}

// ============================================================================
//
// TEXT ROUTINES (if OEFLAG_TEXT)
// 
// ============================================================================

// -------------------
// GetRunObjectFont
// -------------------
// Return the font used by the object.
// 
/*

  // Note: do not forget to enable the functions in the .def file 
  // if you remove the comments below.

void WINAPI GetRunObjectFont(LPRDATA rdPtr, LOGFONT* pLf)
{
	// Example
	// -------
	// GetObject(rdPtr->m_hFont, sizeof(LOGFONT), pLf);
}

// -------------------
// SetRunObjectFont
// -------------------
// Change the font used by the object.
// 
void WINAPI SetRunObjectFont(LPRDATA rdPtr, LOGFONT* pLf, RECT* pRc)
{
	// Example
	// -------
//	HFONT hFont = CreateFontIndirect(pLf);
//	if ( hFont != NULL )
//	{
//		if (rdPtr->m_hFont!=0)
//			DeleteObject(rdPtr->m_hFont);
//		rdPtr->m_hFont = hFont;
//		SendMessage(rdPtr->m_hWnd, WM_SETFONT, (WPARAM)rdPtr->m_hFont, FALSE);
//	}

}

// ---------------------
// GetRunObjectTextColor
// ---------------------
// Return the text color of the object.
// 
COLORREF WINAPI GetRunObjectTextColor(LPRDATA rdPtr)
{
	// Example
	// -------
	return 0;	// rdPtr->m_dwColor;
}

// ---------------------
// SetRunObjectTextColor
// ---------------------
// Change the text color of the object.
// 
void WINAPI SetRunObjectTextColor(LPRDATA rdPtr, COLORREF rgb)
{
	// Example
	// -------
	rdPtr->m_dwColor = rgb;
	InvalidateRect(rdPtr->m_hWnd, NULL, TRUE);
}
*/


// ============================================================================
//
// WINDOWPROC (interception of messages sent to hMainWin and hEditWin)
//
// Do not forget to enable the WindowProc function in the .def file if you implement it
// 
// ============================================================================
/*
// Get the pointer to the object's data from its window handle
// Note: the object's window must have been subclassed with a
// callRunTimeFunction(rdPtr, RFUNCTION_SUBCLASSWINDOW, 0, 0);
// See the documentation and the Simple Control example for more info.
//
LPRDATA GetRdPtr(HWND hwnd, LPRH rhPtr)
{
	return (LPRDATA)GetProp(hwnd, (LPCSTR)rhPtr->rh4.rh4AtomRd);
}

// Called from the window proc of hMainWin and hEditWin.
// You can intercept the messages and/or tell the main proc to ignore them.
//
LRESULT CALLBACK DLLExport WindowProc(LPRH rhPtr, HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	LPRDATA rdPtr = NULL;

	switch (nMsg) {

	// Example
	case WM_CTLCOLORSTATIC:
		{
			// Get the handle of the control
			HWND hWndControl = (HWND)lParam;

			// Get a pointer to the RUNDATA structure (see GetRdptr function above for more info)
			rdPtr = GetRdPtr(hWndControl, rhPtr);

			// Check if the rdPtr pointer is valid and points to an object created with this extension
			if ( rdPtr == NULL || rdPtr->rHo.hoIdentifier != IDENTIFIER )
				break;

			// OK, intercept the message
			HDC hDC = (HDC)wParam;
			SetBkColor(hDC, rdPtr->backColor);
			SetTextColor(hDC, rdPtr->fontColor);
			rhPtr->rh4.rh4KpxReturn = (long)rdPtr->hBackBrush;
			return REFLAG_MSGRETURNVALUE;
		}
		break;
	}

	return 0;
}
*/

// ============================================================================
//
// DEBUGGER ROUTINES
// 
// ============================================================================

// -----------------
// GetDebugTree
// -----------------
// This routine returns the address of the debugger tree
//
LPWORD WINAPI DLLExport GetDebugTree(LPRDATA rdPtr)
{
#if !defined(RUN_ONLY)
	return DebugTree;
#else
	return NULL;
#endif // !defined(RUN_ONLY)
}

// -----------------
// GetDebugItem
// -----------------
// This routine returns the text of a given item.
//
void WINAPI DLLExport GetDebugItem(LPTSTR pBuffer, LPRDATA rdPtr, int id)
{
#if !defined(RUN_ONLY)

	// Example
	// -------
/*
	char temp[DB_BUFFERSIZE];

	switch (id)
	{
	case DB_CURRENTSTRING:
		LoadString(hInstLib, IDS_CURRENTSTRING, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->text);
		break;
	case DB_CURRENTVALUE:
		LoadString(hInstLib, IDS_CURRENTVALUE, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->value);
		break;
	case DB_CURRENTCHECK:
		LoadString(hInstLib, IDS_CURRENTCHECK, temp, DB_BUFFERSIZE);
		if (rdPtr->check)
			wsprintf(pBuffer, temp, _T("TRUE"));
		else
			wsprintf(pBuffer, temp, _T("FALSE"));
		break;
	case DB_CURRENTCOMBO:
		LoadString(hInstLib, IDS_CURRENTCOMBO, temp, DB_BUFFERSIZE);
		wsprintf(pBuffer, temp, rdPtr->combo);
		break;
	}
*/

#endif // !defined(RUN_ONLY)
}

// -----------------
// EditDebugItem
// -----------------
// This routine allows to edit editable items.
//
void WINAPI DLLExport EditDebugItem(LPRDATA rdPtr, int id)
{
#if !defined(RUN_ONLY)

	// Example
	// -------
/*
	switch (id)
	{
	case DB_CURRENTSTRING:
		{
			EditDebugInfo dbi;
			char buffer[256];

			dbi.pText=buffer;
			dbi.lText=TEXT_MAX;
			dbi.pTitle=NULL;

			strcpy(buffer, rdPtr->text);
			long ret=callRunTimeFunction(rdPtr, RFUNCTION_EDITTEXT, 0, (LPARAM)&dbi);
			if (ret)
				strcpy(rdPtr->text, dbi.pText);
		}
		break;
	case DB_CURRENTVALUE:
		{
			EditDebugInfo dbi;

			dbi.value=rdPtr->value;
			dbi.pTitle=NULL;

			long ret=callRunTimeFunction(rdPtr, RFUNCTION_EDITINT, 0, (LPARAM)&dbi);
			if (ret)
				rdPtr->value=dbi.value;
		}
		break;
	}
*/
#endif // !defined(RUN_ONLY)
}


