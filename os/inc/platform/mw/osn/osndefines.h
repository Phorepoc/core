/* SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 * Modified by The Phorepoc Authors (2026): the __GNUC__ branch also defines OSN_NONSHARABLE_CLASS (as the CodeWarrior
 * branch does); Nokia built mmuifw with RVCT, so GCC never had it. Exported over mmuifw's osn_global_api copy (pdk
 * tools/epoc32_export.py: os/inc is copied last). */
/*
* Copyright (c) 2007 Nokia Corporation and/or its subsidiary(-ies). 
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:  Most basic macro definitions
 *
*/



#ifndef OSNDEFINES_H
#define OSNDEFINES_H



/**
 * Defines Keyword that can be used to check if OSN is in use
 */
#define __OSN__

#if defined(__VC32__)
#define OSN_IMPORT __declspec(dllexport)
#define OSN_EXPORT __declspec(dllexport)
#endif

#if defined(__CW32__)
#define OSN_IMPORT __declspec(dllexport)
#define OSN_EXPORT __declspec(dllexport)
#define OSN_NONSHARABLE_CLASS(x) class x
#endif

#if defined(__GNUC__)
#define OSN_IMPORT
#define OSN_EXPORT
#define EXPORT_C
#define IMPORT_C
#define OSN_NONSHARABLE_CLASS(x) class x
#endif

#if defined(__GCC32__) || defined(__GCCE__)
#define OSN_IMPORT
#if !defined(__WINS__) && defined(_WIN32) // VC++ Browser Fix
#define OSN_EXPORT
#else
#define OSN_EXPORT __declspec(dllexport)
#endif
#elif defined (__ARMCC_VERSION) // ARM RCVT
#define OSN_IMPORT __declspec(dllimport)
#define OSN_EXPORT __declspec(dllexport)
#define OSN_NONSHARABLE_CLASS(x) class __declspec(notshared) x
#endif


#endif // OSNDEFINES_H
