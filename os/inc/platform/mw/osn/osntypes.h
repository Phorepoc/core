/* SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 * Modified by The Phorepoc Authors (2026): the __GCC32__ branch types char16 as wchar_t. __wchar_t was the
 * built-in of the GCC 2.9 toolchain that macro once named; our modern GCC knows wchar_t only (as GCCE did).
 * Exported over mmuifw's osn_global_api copy (pdk tools/epoc32_export.py: os/inc is copied last). */
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
* Description:  Defines basic types.
 *
*/



#ifndef OSNTYPES_H
#define OSNTYPES_H

namespace osncore
{

/**
 * Defines a signed 8 bit integer.
 */
typedef signed char int8;

/**
 * Defines an unsigned 8 bit integer.
 */
typedef unsigned char uint8;

/**
 * Defines a signed 16 bit integer.
 */
typedef signed short int int16;

/**
 * Defines an unsigned 16 bit integer.
 */
typedef unsigned short int uint16;

/**
 * Defines an signed 32 bit integer.
 */
typedef signed long int int32;

/**
 * Defines an unsigned 32 bit integer.
 */
typedef unsigned long int uint32;

/**
 * Defines a signed 64 bit integer.
 */
typedef signed long long int64;

/**
 * Defines an unsigned 64 bit integer.
 */
typedef unsigned long long uint64;

/**
 * Defines a 32 bit floating point number.
 */
typedef float float32;

/**
 * Defines a 64 bit floating point number.
 */
typedef double float64;

/**
 * Defines an 8 bit character (usually ASCII).
 */
typedef char char8;

/**
 * Defines an system dependent unsigned int
 * (in 32 bit system it is 4 bytes and in 16 bit system it is 2 bytes).
 */
typedef unsigned int        uint;

/**
 * Defines an 8 bit unsigned char
 */
typedef unsigned char       uchar;



/**
 * Defines a 16 bit character (usually unicode).
 */
#if defined(__VC32__)
typedef uint16 char16;      //here a uint16 is needed because the vc
//version of L"..." returns an unsigned short
#elif defined(__CW32__)
typedef uint16 char16;      //the same like for __VC32__
#elif defined(__GCC32__)
typedef wchar_t char16;     //when using gcc the wchar_t type must be
//used or else L in front of ASCI strings
//(e.g. L"xyz") can't be used to assign a
//unicode string to a char16[]
#elif defined(__ARMCC_VERSION) // RVCT ARM COMPILER
typedef wchar_t char16;
#elif defined(__GCCE__)
typedef wchar_t char16;
#elif defined(__GNUC__)
typedef uint16 char16;
#endif

/**
 * Defines a 32 bit boolean.
 */
typedef int32 bool32;

/**
 * Defines the return code data type.
 */
typedef int32 ReturnCode;


class CPoint;

/**
 *  @class CSize osntypes.h "osn/osntypes.h"
 *  Defines a two dimensional size data type.
 *
 *  @since S60 5.0
 *  @status Draft
 *  @interfaces CSize
 */
class CSize
    {
public:
    /**
     * Default constructor, initializing the mX and mY members to zero.
     */
    inline CSize();

    /**
     * Parameterized constructor.
     *
     * @param aX The first dimension of the size ( usually the width ).
     * @param aY The second dimension of the size ( usually the height ).
     */
    inline CSize( int32 aX, int32 aY );

    /**
     * Copy constructor.
     *
     * @param aSize The size object to be copied.
     */
    inline CSize( const CSize& aSize );

    /**
     * Gets a CPoint whose coordinates are the width and height of this
     * CSize.
     *
     * @return The coordinates of this CSize converted to a point.
     */
    inline CPoint AsPoint();

    /**
     * Compares this CSize with the specified CSize for equality.
     *
     * For two CSizes to be equal, both their width and height values
     * must be equal.
     *
     * @param aSize The CSize to be compared with this CSize.
     *
     * @return  TRUE, if the sizes are equal,  FALSE otherwise.
     */
    inline bool32 operator==( const CSize& aSize ) const;

    /**
     * Compares this CSize with the specified CSize for inequality.
     *
     * For two CSize to be unequal, either their width or height values
     * must be different.
     *
     * @param aSize The CSize to be compared with this CSize.
     *
     * @return  TRUE, if the sizes are different,  FALSE
     *         otherwise.
     */
    inline bool32 operator!=( const CSize& aSize ) const;

    /**
     * CSize addition assignment operator.
     *
     * The operator adds the specified CSize to this CSize, and
     * assigns the result back to this CSize.
     *
     * @param aSize The CSize to be added.
     *
     * @return A reference to this point object.
     */
    inline CSize& operator+=( const CSize& aSize );

    /**
     * CSize subtraction assignment operator.
     *
     * The operator substracts the specified CSize from this CSize,
     * and assigns the result back to this CSize.
     *
     * @param aSize The CSize to be substracted.
     *
     * @return A reference to this point object.
     */
    inline CSize& operator-=( const CSize& aSize );

    int32 mX; ///< The first dimension.
    int32 mY; ///< The second dimension.
    };

/**
 * CSize addition operator.
 *
 * Calculates the sum of two CSize objects
 *
 * @param aLeftHandSide The left addend.
 * @param aRightHandSide The right addend.
 *
 * @return The sum.
 *
 */
inline const CSize operator+( const CSize& aLeftHandSide,
                              const CSize& aRightHandSide );

/**
 * CSize substraction operator.
 *
 * Calculates the difference of two CSize objects
 *
 * @param aLeftHandSide The minuend.
 * @param aRightHandSide The subtrahend.
 *
 * @return The difference.
 *
 */
inline const CSize operator-( const CSize& aLeftHandSide,
                              const CSize& aRightHandSide );


/**
 *  @class CPoint osntypes.h "osn/osntypes.h"
 *  Defines a two dimensional point data type.
 *
 *  @since S60 5.0
 *  @status Draft
 *  @interfaces CPoint
 */
class CPoint
    {
public:
    /**
     * Default constructor, initializing the mX and mY members to zero.
     */
    inline CPoint(  );

    /**
     * Parameterized constructor.
     *
     * @param aX The first coordinate of the point.
     * @param aY The second coordinate of the point.
     */
    inline CPoint( int32 aX, int32 aY );

    /**
     * Copy constructor.
     *
     * @param aPoint The point object to be copied.
     */
    inline CPoint( const CPoint& aPoint );

    /**
     * Gets a CSize whose width and height are the coordinates of this
     * CPoint.
     *
     * @return The width and height of this CPoint converted to a size.
     */
    inline CSize AsSize();

    /**
     * Compares two points for equality.
     *
     * For two points to be equal, both their x and y coordinate values
     * must be equal.
     *
     * @param aPoint The point to be compared with this point.
     *
     * @return  TRUE, if the points are equal,  FALSE otherwise.
     */
    inline bool32 operator==( const CPoint& aPoint ) const;

    /**
     * Compares two points for inequality.
     *
     * For two points to be unequal, either their x or their y
     * coordinate values must be different.
     *
     * @param aPoint The point to be compared with this point.
     *
     * @return  TRUE, if the points are different,  FALSE
     *         otherwise.
     */
    inline bool32 operator!=( const CPoint& aPoint ) const;

    /**
     * CPoint addition assignment operator.
     *
     * The operator adds the specified CPoint to this CPoint, and
     * assigns the result back to this CPoint.
     *
     * @param aPoint The CPoint to be added.
     *
     * @return A reference to this point object.
     */
    inline CPoint& operator+=( const CPoint& aPoint );

    /**
     * CPoint subtraction assignment operator.
     *
     * The operator substracts the specified CPoint from this CPoint,
     * and assigns the result back to this CPoint.
     *
     * @param aPoint The CPoint to be substracted.
     *
     * @return A reference to this point object.
     */
    inline CPoint& operator-=( const CPoint& aPoint );

    int32 mX; ///< The x-coordinate position of the point.
    int32 mY; ///< The y-coordinate position of the point.
    };

/**
 * CPoint addition operator.
 *
 * Calculates the sum of two CPoint objects
 *
 * @param aLeftHandSide The left addend.
 * @param aRightHandSide The right addend.
 *
 * @return The sum.
 *
 */
inline const CPoint operator+( const CPoint& aLeftHandSide,
                               const CPoint& aRightHandSide );

/**
 * CPoint substraction operator.
 *
 * Calculates the difference of two CPoint objects
 *
 * @param aLeftHandSide The minuend.
 * @param aRightHandSide The subtrahend.
 *
 * @return The difference.
 *
 */
inline const CPoint operator-( const CPoint& aLeftHandSide,
                               const CPoint& aRightHandSide );

/**
 *  @class CRect osntypes.h "osn/osntypes.h"
 *  Defines a two dimensional rectangle data type.
 *
 *  @since S60 5.0
 *  @status Draft
 *  @interfaces CRect
 */
class CRect
    {
public:
    /**
     * Default constructor, initializing the coordinates of the top left
     * corner and the size to ( 0,0 ).
     */
    inline CRect();

    /**
     * Parameterized constructor.
     *
     * @param aX The first coordinate of the rectangle's top left point.
     * @param aY The second coordinate of the rectangle's top left
     *           point.
     * @param aWidth The width of the rectangle.
     * @param aHeight The height of the rectangle.
     */
    inline CRect( int32 aX, int32 aY, int32 aWidth, int32 aHeight );

    /**
     * Parameterized constructor.
     *
     * Constructs the rectangle with a point ( top left corner ) and a
     * size.
     *
     * @param aTopLeft The top left point of the rectangle.
     * @param aSize The size of the rectangle.
     */
    inline CRect( const CPoint& aTopLeft, const CSize& aSize );

    /**
     * Parameterized constructor.
     *
     * Constructs the rectangle with a size. The top left corner is set
     * to (0,0).
     *
     * @param aSize The size of the rectangle.
     */
    inline CRect( const CSize& aSize );

    /**
     * Copy constructor.
     *
     * @param aRect The rectangle object to be copied.
     */
    inline CRect( const CRect& aRect );

    /**
     * Determines whether the area covered by the rectangle is zero.
     *
     * @return  TRUE, if the rectangle is empty,  FALSE
     *         otherwise.
     */
    inline bool32 IsEmpty() const;

    /**
     * Tests whether this rectangle overlaps with the specified
     * rectangle.
     *
     * Two rectangles overlap if any point is located within both
     * rectangles. There is no intersection if two adjacent sides touch
     * without overlapping, or if either rectangle is empty.
     *
     * @param aRect The rectangle to compare with this rectangle for an
     *              intersection.
     *
     * @return  TRUE, if the two rectangles overlap,  FALSE
     *         otherwise.
     */
    inline bool32 Intersects( CRect aRect ) const;

    /**
     * Tests whether this normalized rectangle overlaps with the
     * specified normalized rectangle.
     *
     * Two rectangles overlap if any point is located within both
     * rectangles. There is no intersection if two adjacent sides touch
     * without overlapping, or if either rectangle is empty.
     * If one of the rectangles isn't normalized, then the function also
     * returns  FALSE,
     *
     * Because of performance reasons this function should favored over
     * the Intersects function, when it is certain that both rectangles
     * are normalized.
     *
     * @param aRect The rectangle to compare with this rectangle for an
     *              intersection.
     *
     * @return  TRUE, if the two rectangles are normalized and
     *         overlap,  FALSE otherwise.
     */
    inline bool32 FastIntersects( const CRect& aRect ) const;

    /**
     * Gets the normalized area of intersection between this rectangle
     * and the specified rectangle, and assigns it to this rectangle.
     *
     * If the two rectangles do not intersect, then, on return, this
     * rectangle is set to be empty.
     *
     * @param aRect The rectangle to be used with this rectangle to get
     *              the area of intersection.
     */
    inline void Intersection( CRect aRect );

    /**
     * Gets the normalized area of intersection between this normalized
     * rectangle and the specified normalized rectangle, and assigns it
     * to this rectangle.
     *
     * If the two rectangles do not intersect or are not normalized,
     * then, on return, this rectangle is set to be empty.
     *
     * Because of performance reasons this function should be favored
     * over the Intersection() function, when it is certain that both
     * rectangles are normalized.
     *
     * @param aRect The rectangle to be used with this rectangle to get
     *              the area of intersection.
     */
    inline void FastIntersection( const CRect& aRect );

    /**
     * Ensures that the rectangle's width and height have positive
     * values.
     *
     * For example, if the rectangle's co-ordinates are such that the
     * top is below the bottom, or the right hand side is to the left of
     * the left hand side, normalisation swaps the co-ordinates of the
     * top and bottom or of the left and right.
     */
    inline void Normalize();

    /**
     * Tests whether the rectangle is normalized.
     *
     * A rectangle is normalized, when its width and height are both
     * greater than or equal to zero.
     */
    inline bool32 IsNormalized() const;

    /**
     * Compares two rectangles for equality.
     *
     * For two rectangles to be equal, the coordinates of their top left
     * corners and their sizes must be equal.
     *
     * @param aRect The rectangle to compare with this rectangle.
     *
     * @return  TRUE, if the rectangles are equal,  FALSE
     *         otherwise.
     */
    inline bool32 operator==( const CRect& aRect ) const;

    /**
     * Compares two rectangles for inequality.
     *
     * Two rectangles are unequal if the coordinates of their top left
     * corners or their sizes differ.
     *
     * @param aRect The rectangle to compare with this rectangle.
     *
     * @return  TRUE, if the rectangles are different,  FALSE
     *         otherwise.
     */
    inline bool32 operator!=( const CRect& aRect ) const;

    /**
     * CPoint addition assignment operator.
     *
     * The operator moves this CRect's offset by the given point's
     * coordinates.
     *
     * @param aPoint The CPoint to be added.
     *
     * @return A reference to this rectangle object.
     */
    inline CRect& operator+=( const CPoint& aPoint );

    /**
     * CPoint subtraction assignment operator.
     *
     * The operator moves this CRect's offset by the given point's
     * coordinates.
     *
     * @param aPoint The CPoint to be substracted.
     *
     * @return A reference to this rectangle object.
     */
    inline CRect& operator-=( const CPoint& aPoint );

    /**
     * CSize addition assignment operator.
     *
     * The operator increases this CRect's size by the given size.
     *
     * @param aSize The CSize to be added.
     *
     * @return A reference to this rectangle object.
     */
    inline CRect& operator+=( const CSize& aSize );

    /**
     * CSize subtraction assignment operator.
     *
     * The operator decreases this CRect's size by the given size.
     *
     * @param aSize The CSize to be substracted.
     *
     * @return A reference to this rectangle object.
     */
    inline CRect& operator-=( const CSize& aSize );

    int32   mX;      ///< The position of the left-hand side of the
    ///< rectangle.
    int32   mY;      ///< The position of the top side of the rectangle.
    int32   mWidth;  ///< The width of the rectangle.
    int32   mHeight; ///< The height of the rectangle.
    };

/**
 * CRect + CPoint addition operator.
 *
 * The operator copies the CRect and moves the copy's offset by the given
 * point's coordinates.
 *
 * @param aRect The CRect addend.
 * @param aPoint The CPoint to be added.
 *
 * @return The moved CRect.
 *
 */
inline const CRect operator+( const CRect& aRect,
                              const CPoint& aPoint );

/**
 * CRect - CPoint subtraction operator.
 *
 * The operator copies the CRect and moves the copy's offset by the
 * given point's coordinates.
 *
 * @param aRect The CRect addend.
 * @param aPoint The CPoint to be substracted.
 *
 * @return The moved CRect.
 *
 */
inline const CRect operator-( const CRect& aRect,
                              const CPoint& aPoint );

/**
 * CRect + CSize addition operator.
 *
 * The operator copies the CRect and resizes the copy by the given size.
 *
 * @param aRect The CRect addend.
 * @param aSize The CSize to be added.
 *
 * @return The resized CRect.
 *
 * @ingroup Runtime
 */
inline const CRect operator+( const CRect& aRect,
                              const CSize& aSize );

/**
 * CRect - CSize subtraction operator.
 *
 * The operator copies the CRect and resizes the copy by the given size.
 *
 * @param aRect The CRect minuend.
 * @param aSize The CSize to be substracted.
 *
 * @return The resized CRect.
 *
 * @ingroup Runtime
 */
inline const CRect operator-( const CRect& aRect,
                              const CSize& aSize );

/**
  * Defines maximum value for a int8 variable
  */
const int8 MAX_INT8      = 0x7f;

/**
 * Defines minimum value for a int8 variable
 */
const int8 MIN_INT8      = 0x80;

/**
 * Defines maximum value for a uint8 variable
 */
const uint8 MAX_UINT8    = 0xffu;

/**
 * Defines maximum value for a int16 variable
 */
const int16 MAX_INT16    = 0x7fff;

/**
 * Defines minimum value for a int16 variable
 */
const int16 MIN_INT16    = 0x8000;

/**
 * Defines maximum value for a uint16 variable
 */
const uint16 MAX_UINT16  = 0xffffu;

/**
 * Defines maximum value for a int32 variable
 */
const int32 MAX_INT32    = 0x7fffffff;

/**
 * Defines minimum value for a int32 variable
 */
const int32 MIN_INT32    = 0x80000000;

/**
 * Defines maximum value for a uint32 variable
 */
const uint32 MAX_UINT32  = 0xffffffffu;


/**
 * Maximum path/filename length including a NULL terminator. A char16*
 * array of this size is guaranteed to accomodate all filenames returned
 * by the system.
 */
const uint32 MAX_PATH_LENGTH = 257;

/// Filename path delimiter.
const char16 PATH_DELIMITER = '\\';

/// Filename drive delimiter.
const char16 DRIVE_DELIMITER = ':';

/// Filename extension delimiter.
const char16 EXTENSION_DELIMITER = '.';


#include <osn/osntypes.inl>
}
#endif
