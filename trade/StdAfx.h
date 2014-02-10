// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__681373C3_0AC0_11D6_B0ED_00B0D074179C__INCLUDED_)
#define AFX_STDAFX_H__681373C3_0AC0_11D6_B0ED_00B0D074179C__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC OLE automation classes
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <afxsock.h>		// MFC socket extensions
//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.
#endif // !defined(AFX_STDAFX_H__681373C3_0AC0_11D6_B0ED_00B0D074179C__INCLUDED_)
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#include <afxinet.h>
#include "Weixin.h"
#include <Gdiplus.h>
#include <atlimage.h> 
#pragma comment(lib, "Gdiplus.lib")
using namespace Gdiplus;