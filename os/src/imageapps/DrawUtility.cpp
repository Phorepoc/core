/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors: Juha Kauppinen, Mika Hokkanen
* 
* Description: Photo Browser
*
*/

#include "DrawUtility.h"
#include "debug.h"

const TInt KTNSize = 10;

/*--------------------------------------------------------------------------*/
// Class constants

// Mesh data

// Triangle count
const int CDrawUtility::iTriCount = 4;

// Vertices
const GLfixed CDrawUtility::iVertices[4*3]=
	{
	// Tetrahedron coordinates
	10*(1<<16),		10*(1<<16),		10*(1<<16),
	-10*(1<<16),	-10*(1<<16),	10*(1<<16),
	-10*(1<<16),	10*(1<<16),		-10*(1<<16),
	10*(1<<16),		-10*(1<<16),	-10*(1<<16),
	};

// Normals
const GLfixed CDrawUtility::iNormals[4*3]=
	{
	// Tetrahedron normals
	/*
	Normal is a vector with unit lenght (length=1)
	Unit lenght vector = vector/lenght
	
	Calculate normals from corners, makes gouraud shading
	Vector: (1,1,1)
	Lenght = SQRT( 1*1 + 1*1 + 1*1 ) = SQRT(3) = 1.732050808
	1/Lenght = 0.577350269
	In fixed point: 0.577350269*(1<<16) = 37837
	*/
	37837,	37837,	37837,
	-37837,	-37837,	37837,
	-37837,	37837,	-37837,
	37837,	-37837,	-37837,
	};

// Indexes define which vertices make a triangle
// These are just indexes to iVertices table
const GLushort CDrawUtility::iIndices[4*3]=
	{
	0,2,3,
	0,1,3,
	0,1,2,
	2,1,3,
	};

/*--------------------------------------------------------------------------*/
// NewL
//
//CDrawableInterface* CDrawUtility::NewL(void)
CDrawUtility* CDrawUtility::NewL(CImagicContainerBrowser* aContainer)
	{
	// Create object
	CDrawUtility* self = new(ELeave) CDrawUtility();
	
	// Call 2nd stage constructor
	CleanupStack::PushL(self);
	self->ConstructL(aContainer);
	CleanupStack::Pop(self);
	
	return self;
	}

/*--------------------------------------------------------------------------*/
// Constructor
//
CDrawUtility::CDrawUtility() :
		//CDrawableInterface(),
		iAngleX(0),
		iAngleY(0),
		iAngleZ(0)
	{
	// Nothing here
	}

/*--------------------------------------------------------------------------*/
// Second stage constructor
//
void CDrawUtility::ConstructL(CImagicContainerBrowser* aContainer)
	{
	iContainer = aContainer;
	iRotation = 0;
	iRotationTarget = 0;
	
	float ambient[4]={0.3,0.3,0.3, 1};
	float diffuse[4]={1, 1, 1, 1};
    glLightfv(GL_LIGHT0,GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0,GL_DIFFUSE, diffuse);
	}

/*--------------------------------------------------------------------------*/
// Destructor
//
CDrawUtility::~CDrawUtility()
	{
	// Nothing here
	}

/*--------------------------------------------------------------------------*/
// Makes sure that angle is on valid range
//
void CDrawUtility::LimitAngle(TInt &Angle)
	{
	while (Angle<0)
		Angle+=360;
	while (Angle>360)
		Angle-=360;
	}

/*--------------------------------------------------------------------------*/
// Update animation
// Returns true if screen should be redrawn
//
TBool CDrawUtility::Update(void)
	{
	// Rotate
	iAngleX+=4;
	iAngleY+=2;
	iAngleZ-=2;
	
	// Check limist
	LimitAngle(iAngleX);
	LimitAngle(iAngleY);
	LimitAngle(iAngleZ);
	
	// Since this is loading animation, this doesn't want screen to be updated
	return EFalse;
	}



/*--------------------------------------------------------------------------*/
// Draws Zoom image thumbnail
//
void CDrawUtility::DrawZoomIcon(    const CImageData* aImageData,
                                    const TSize aScreenSize, 
                                    float aDrawOneByOneX, 
                                    float aDrawOneByOneY,
                                    TReal aDrawOnebyOneW, 
                                    TReal aDrawOnebyOneH,
                                    TBool aShowLocationRect)
    {
    
    iScrAspectratio = (TReal)aScreenSize.iWidth/(TReal)aScreenSize.iHeight;
    
    if(aImageData->iGridData.iGlLQ128TextIndex != 0)
        {
        iDrawOneByOneX = aDrawOneByOneX; 
        iDrawOneByOneY = aDrawOneByOneY;
        iDrawOnebyOneW = aDrawOnebyOneW; 
        iDrawOnebyOneH = aDrawOnebyOneH;
                                            
        iAspectRatio = aImageData->GetAspectRatio();
        iScrSize = aScreenSize;
        
        //Define thumbnail size dependinf on screen orientation
        if(iAspectRatio > 1)
            {
            iThumbSize.iWidth=(iScrSize.iWidth/KTNSize);
            iThumbSize.iHeight=iThumbSize.iWidth/iAspectRatio;
            iZoomRectSize.iWidth = iScrSize.iWidth/KTNSize;
            iZoomRectSize.iHeight = iZoomRectSize.iWidth/iScrAspectratio;
            }
        else
            {
            iThumbSize.iWidth=(iScrSize.iHeight/KTNSize);
            iThumbSize.iHeight=iThumbSize.iWidth/iAspectRatio;
            iZoomRectSize.iHeight = iScrSize.iHeight/KTNSize;
            iZoomRectSize.iWidth = iZoomRectSize.iHeight*iScrAspectratio;
            }
        
        GLfixed vertices[8];
        SetPictureVertices(vertices, iAspectRatio);
        
        glColor4f(1,1,1, 1);
        
        // Set OpenGL state
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        glVertexPointer( 2, GL_FIXED, 0, vertices );

        glPushMatrix();
        
        // Set ortho to match screen size
        glLoadIdentity();
        glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -50,50);
        
        //Move to top right corner and leave some space around "frame"
        if(iContainer->GetScreenOrientation())
            {
            iRotationTarget = 0 - (TReal)aImageData->GetOrientation();
            
            if(iAspectRatio > 1 && iRotationTarget == 0)
                glTranslatef(iThumbSize.iWidth+4, iScrSize.iHeight-iThumbSize.iHeight-4, 1);
            else if(iAspectRatio < 1 && iRotationTarget == 0)
                glTranslatef((iThumbSize.iWidth)*iAspectRatio+4, iScrSize.iHeight-iThumbSize.iWidth-4, 1);
            else// if(iAspectRatio < 1 && iRotationTarget == 0)
                glTranslatef((iThumbSize.iHeight)+4, iScrSize.iHeight-iThumbSize.iWidth-4, 1);
            }
        else
            {
            iRotationTarget = -90 - (TReal)aImageData->GetOrientation();
            
            if(iAspectRatio > 1 && iRotationTarget == -90)
                glTranslatef(iScrSize.iWidth-iThumbSize.iHeight-4, iScrSize.iHeight-iThumbSize.iWidth-4, 1);
            else if(iAspectRatio < 1 && iRotationTarget == -90)
                glTranslatef(iScrSize.iWidth-(iThumbSize.iHeight*iAspectRatio)-4, iScrSize.iHeight-(iThumbSize.iWidth*iAspectRatio)-4, 1);
            else// if(iAspectRatio < 1 && iRotationTarget == -90)
                glTranslatef(iScrSize.iWidth-(iThumbSize.iHeight*iAspectRatio)-4, iScrSize.iHeight-(iThumbSize.iHeight)-4, 1);
            }
        
        iContainer->Interpolate(iRotation, iRotationTarget, 0.60);
        glRotatef(iRotation, 0,0,1);
        
        DrawFrame(0);
        
        glBindTexture(GL_TEXTURE_2D, aImageData->iGridData.iGlLQ128TextIndex);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
                        
        // Restore OpenGL state
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        
        //glDisable(GL_BLEND);
        if(aShowLocationRect)
            DrawZoomFrame(iRotationTarget);
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glPopMatrix();
        }
    }

//Shows rectangle for zoomed are
void CDrawUtility::DrawZoomFrame(float aRotationTarget)
    {
    DP0_IMAGIC(_L("CImagicContainerBrowser::DrawFaceFrame++"));
    DP1_IMAGIC(_L("CImagicContainerBrowser::DrawZoomFrame - aRotationTarget: %f"),aRotationTarget);
    
    GLfixed vertices[8];
    glPushMatrix();
    glDisable(GL_TEXTURE_2D);
    glTranslatef(0, 0, 0.01f);
    glColor4f(0,0.7,0,1);
    glLineWidth(2.0f);
    glVertexPointer(2, GL_FIXED, 0, vertices);
    
	TReal xPos = iDrawOneByOneX*(TReal)iThumbSize.iWidth*2;
	TReal yPos = iDrawOneByOneY*(TReal)iThumbSize.iHeight*2*iAspectRatio;
	
	//float x,y;
	TInt x,y;
	TInt rotationTarget = aRotationTarget;
	rotationTarget%=360;
	
	if(rotationTarget == 0)
	    {
	    glTranslatef(-xPos, yPos, 0);
	    if(iAspectRatio > 1)
	        {
	        x=iDrawOnebyOneW*iZoomRectSize.iWidth * (1<<16);
	        y=iDrawOnebyOneW*iZoomRectSize.iHeight * (1<<16);
	        }
	    else
	        {
	        x=iDrawOnebyOneH*iZoomRectSize.iWidth * (1<<16);
            y=iDrawOnebyOneH*iZoomRectSize.iHeight * (1<<16);
	        }
	        
	    }
	else if(rotationTarget == -90)
        {
        glTranslatef(-yPos, -xPos, 0);
        if(iAspectRatio > 1)
            {
            y=iDrawOnebyOneH*iScrAspectratio*iZoomRectSize.iWidth * (1<<16);
            x=iDrawOnebyOneH*iScrAspectratio*iZoomRectSize.iHeight * (1<<16);
            }
        else
            {
            y=iDrawOnebyOneH*iZoomRectSize.iWidth * (1<<16);
            x=iDrawOnebyOneH*iZoomRectSize.iHeight * (1<<16);
            }
        }
	else if(rotationTarget == -180)
        {
        glTranslatef(xPos, -yPos, 0);
        if(iAspectRatio > 1)
            {
            x=iDrawOnebyOneW*iZoomRectSize.iWidth * (1<<16);
            y=iDrawOnebyOneW*iZoomRectSize.iHeight * (1<<16);
            }
        else
            {
            x=iDrawOnebyOneH*iZoomRectSize.iWidth * (1<<16);
            y=iDrawOnebyOneH*iZoomRectSize.iHeight * (1<<16);
            }
        }
	else if(rotationTarget == -270)
        {
        glTranslatef(yPos, xPos, 0);
        if(iAspectRatio > 1)
            {
            y=iDrawOnebyOneH*iScrAspectratio*iZoomRectSize.iWidth * (1<<16);
            x=iDrawOnebyOneH*iScrAspectratio*iZoomRectSize.iHeight * (1<<16);
            }
        else
            {
            y=iDrawOnebyOneH*iZoomRectSize.iWidth * (1<<16);
            x=iDrawOnebyOneH*iZoomRectSize.iHeight * (1<<16);
            }
        }

	TInt tnSizeFixedH = iThumbSize.iHeight*(1<<16);
	TInt tnSizeFixedW = iThumbSize.iWidth*(1<<16);
	
	if(y > tnSizeFixedH)
	    y=tnSizeFixedH;
	if(x > tnSizeFixedW)
        x=tnSizeFixedW;
	
	vertices[0*2+0] = -x;	vertices[0*2+1] = -y;
	vertices[1*2+0] =  x;	vertices[1*2+1] = -y;
	vertices[2*2+0] =  x;	vertices[2*2+1] =  y;
	vertices[3*2+0] = -x;	vertices[3*2+1] =  y;
	
    glDrawArrays(GL_LINE_LOOP,0,4);

    glColor4f(1,1,1,1);
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
	
    DP0_IMAGIC(_L("CImagicContainerBrowser::DrawZoomFrame--"));
    }

/*----------------------------------------------------------------------*/
// Draws background frame
//
void CDrawUtility::DrawFrame(TInt aIndex)
    {

    // Draw frame around selected image
    glPushMatrix();
    glDisable(GL_TEXTURE_2D);
    
    //Frame size
    float scale=1.09;
    glColor4f(1,1,1, 1);
    
    glTranslatef(0,0,-0.03);
    glScalef(scale,scale,scale);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    
    //glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
    }

void CDrawUtility::SetPictureVertices(GLfixed* aVertices, TReal aAspectRatio)
    {
    //DP0_IMAGIC(_L("CImagicContainerBrowser::SetPictureVertices"));
    
    GLfixed vx = (iThumbSize.iWidth)*(1<<16);
    GLfixed vy = (iThumbSize.iWidth)*(1<<16);
    
    
    if(aAspectRatio > 1)
        {
        vy = ((iThumbSize.iWidth)/aAspectRatio)*(1<<16);
        }
    else
        {
        vx = ((iThumbSize.iWidth)*aAspectRatio)*(1<<16);
        }

    
    aVertices[0*2+0] = -vx;
    aVertices[0*2+1] = vy;
    
    aVertices[1*2+0] = vx;
    aVertices[1*2+1] = vy;
    
    aVertices[2*2+0] = -vx;
    aVertices[2*2+1] = -vy;
    
    aVertices[3*2+0] = vx;
    aVertices[3*2+1] = -vy;

    }

#if 1

/*--------------------------------------------------------------------------*/
// Draw moving arrow
//
void CDrawUtility::DrawMovingArrow(TBool aPrevArrow, TBool aUpDownArrow, const TSize& aScreenSize)
    {
    //Define shape of direction array
    const GLfixed vertices[4*2] = 
		{
		0*1<<16,  0*1<<16,
		10*1<<16, 7*1<<16,
		7*1<<16,  0*1<<16,
		10*1<<16,-7*1<<16,
		};
    //Define colors of direction array
    const GLubyte colors[4*4] = 
		{
		//61,174,227, 128,
		/*255,255,255, 255,
		81,194,247, 255,
		61,174,227, 255,
		41,154,207, 255,*/
        255,255,255, 255,
        128,128,128, 255,
        50,50,50, 255,
        128,128,128, 255,
		};
    //And order of drawing
	const GLushort indices[2*3]=
		{
		0,1,2,
		2,3,0,
		};
	
	TReal scale = 1;
	if(aScreenSize.iHeight > 320 || aScreenSize.iWidth > 320)
	    {
	    scale = 1.5;
	    }
	    
    // Set OpenGL state
	// Shade model
    glShadeModel(GL_SMOOTH);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glPushMatrix();
	
#ifdef ENABLE_ALPHA
	// Setup alpha
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif	
    // Set ortho to match screen size
    glLoadIdentity();
    glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -100,100);
    glPushMatrix();

    // Move to arrow position
	int ArrowDistance=5;
	if(aPrevArrow)
		{
		glTranslatef(ArrowDistance,aScreenSize.iHeight/2,0);
        glVertexPointer(2,GL_FIXED,0, vertices);
        glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
        glScalef(scale,scale,scale);
        glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
        glScalef(1/scale,1/scale,1/scale);
		glTranslatef(aScreenSize.iWidth-2*ArrowDistance,0/*aScreenSize.iHeight/2*/,0);
		// Flip arrow around so it points to right
	    glRotatef(180,0,0,1);
		// Also flip it on X
		//glRotatef(180,1,0,0);
		glVertexPointer(2,GL_FIXED,0, vertices);
        glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
        glScalef(scale,scale,scale);
        glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
        glScalef(1/scale,1/scale,1/scale);
		
        glPopMatrix();
		glPushMatrix();
	    }
	
    if(aUpDownArrow)
        {
        glTranslatef(aScreenSize.iWidth/2, ArrowDistance,0);
        //glTranslatef(aScreenSize.iWidth/2,(aScreenSize.iHeight)-ArrowDistance,0);
        // Flip arrow around so it points to right
        glRotatef(90,0,0,1);
        // Also flip it on X
        //glRotatef(180,1,0,0);
        // Draw arrow
        glVertexPointer(2,GL_FIXED,0, vertices);
        glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
        glScalef(scale,scale,scale);
        glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
        glScalef(1/scale,1/scale,1/scale);
        
        glTranslatef(aScreenSize.iHeight-ArrowDistance*2,0,0);
		glRotatef(180,0,0,1);
		
        // Draw arrow
        glVertexPointer(2,GL_FIXED,0, vertices);
        glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
        glScalef(scale,scale,scale);
        glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
        glScalef(1,1,1);
        }
	
    glPopMatrix();
#ifdef ENABLE_ALPHA
	// Remove alpha
	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
#endif
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_FLAT);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glPopMatrix();
    }

/*--------------------------------------------------------------------------*/
// Draw animation
//
void CDrawUtility::Draw(const TSize &aScreenSize)
    {
    // Set OpenGL state
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glPushMatrix();
    
    // Set ortho to match screen size
    glLoadIdentity();
    glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -100,100);
    
    // Move to top right corner
    glTranslatef(aScreenSize.iWidth-40,40,0);
    
    /*
    // Calculate prespective values
    GLfloat aspectRatio = (GLfloat)(aScreenSize.iWidth) / (GLfloat)(aScreenSize.iHeight);
    const float near = 0.001;
    const float far = 100.0;
    const float top = 0.414*near;
    const float bottom = -top;
    const float left = aspectRatio * bottom;
    const float right = aspectRatio * top;
    
    // Set perspective
    glLoadIdentity();
    glFrustumf(left,right, bottom,top, near,far);
    glTranslatef(0,0,-40);
    */
    // Update light direction
    float direction[4]={1, 0, -1, 0};
    glLightfv(GL_LIGHT0,GL_POSITION, direction);
    glColor4f(1,1,1, 1);
    // Apply rotations
    glRotatef(iAngleX, 1,0,0);
    glRotatef(iAngleY, 0,1,0);
    glRotatef(iAngleZ, 0,0,1);
    
    // Draw tetrahedron
    glVertexPointer(3,GL_FIXED,0, iVertices);
	glNormalPointer(GL_FIXED,0, iNormals);
    glDrawElements(GL_TRIANGLES, iTriCount*3,GL_UNSIGNED_SHORT,iIndices);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glPopMatrix();
    }
#endif

/*--------------------------------------------------------------------------*/
// Draw moving arrow
//
void CDrawUtility::DrawMenuIndicators(const TSize& aScreenSize)
    {
    //Define shape of direction array
    const GLfixed vertices[4*2] = 
        {
        0*1<<16,  0*1<<16,
        10*1<<16, 7*1<<16,
        7*1<<16,  0*1<<16,
        10*1<<16,-7*1<<16,
        };
    //Define colors of direction array
    const GLubyte colors[4*4] = 
        {
        //61,174,227, 128,
        255,255,255, 255,
        81,194,247, 255,
        61,174,227, 255,
        41,154,207, 255,
        };
    //And order of drawing
    const GLushort indices[2*3]=
        {
        0,1,2,
        2,3,0,
        };
    
    TReal scale = 1;
    if(aScreenSize.iHeight > 320 || aScreenSize.iWidth > 320)
        {
        scale = 1.5;
        }
        
    // Set OpenGL state
    // Shade model
    glShadeModel(GL_SMOOTH);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glPushMatrix();
    
#ifdef ENABLE_ALPHA
    // Setup alpha
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif  
    // Set ortho to match screen size
    glLoadIdentity();
    glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -100,100);
    glPushMatrix();

    // Move to arrow position
    TInt ArrowDistance=20;
    
    glTranslatef(aScreenSize.iWidth - ArrowDistance, aScreenSize.iHeight/2, 0);
    
    //glTranslatef(ArrowDistance, aScreenSize.iWidth/2,0);
    glVertexPointer(2,GL_FIXED,0, vertices);
    glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
    glScalef(scale,scale,scale);
    glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
    glScalef(1/scale,1/scale,1/scale);
    glTranslatef(aScreenSize.iWidth-2*ArrowDistance, 0, 0);
    
    // Flip arrow around so it points to right
    glRotatef(180,0,0,1);
    
    glVertexPointer(2,GL_FIXED,0, vertices);
    glColorPointer(4,GL_UNSIGNED_BYTE,0, colors);
    glScalef(scale,scale,scale);
    glDrawElements(GL_TRIANGLES, 2*3, GL_UNSIGNED_SHORT, indices);
    glScalef(1/scale,1/scale,1/scale);
    
    
    
    glPopMatrix();
    
#ifdef ENABLE_ALPHA
    // Remove alpha
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
#endif
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_FLAT);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glPopMatrix();
    }

/*--------------------------------------------------------------------------*/
// Draw icon texture
//
void CDrawUtility::DrawIcon(const TSize &aScreenSize, GLuint aTexIndex)
    {
    if(aTexIndex != 0)
        {
        GLfixed vertices[8];
        SetPictureVertices(vertices, 1);
        
        glColor4f(1,1,1, 0.75);
        
        // Set OpenGL state
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        
        //Enable alpha blending
        glEnable(GL_BLEND);
        glEnable(GL_ALPHA_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
        
        
        glVertexPointer( 2, GL_FIXED, 0, vertices );
        
        glPushMatrix();
        
        // Set ortho to match screen size
        glLoadIdentity();
        glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -50,50);
        
        // Move to top right corner
        //glTranslatef(29, 29, 1);
        glTranslatef(0, 0, 0);
        
//#ifdef _S60_5x_ACCELEROMETER_
        if(iContainer->GetScreenOrientation())
            {
            iRotationTarget = 0;
            }
        else
            {
            iRotationTarget = -90;
            }
        iContainer->Interpolate(iRotation, iRotationTarget, 0.25);
        glRotatef(iRotation, 0,0,1);
//#endif
        
        glBindTexture(GL_TEXTURE_2D, aTexIndex);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
                        
        // Restore OpenGL state
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                
        glPopMatrix();
        }
    }


void CDrawUtility::DrawIcon2(const TSize &aScreenSize, GLuint aTexIndex, TReal aAlpha)
    {
    
    iScrAspectratio = (TReal)aScreenSize.iWidth/(TReal)aScreenSize.iHeight;
    
        GLfixed vertices[8];
        SetPictureVertices(vertices, 0.3);
        
        //glColor4f(1,1,1, 1);
        glColor4f(1,1,1, aAlpha);
        // Set OpenGL state
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        
        //Enable alpha blending
        glEnable(GL_BLEND);
        glEnable(GL_ALPHA_TEST);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_COLOR);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                
        glVertexPointer( 2, GL_FIXED, 0, vertices );

        glPushMatrix();
        
        // Set ortho to match screen size
        glLoadIdentity();
        glOrthof(0,aScreenSize.iWidth, aScreenSize.iHeight,0, -50,50);
        
        
        glTranslatef(iScrSize.iWidth-7, iScrSize.iHeight-25, 1);
        iContainer->Interpolate(iRotation, iRotationTarget, 0.60);
        //glRotatef(90, 0,0,1);
        glBindTexture(GL_TEXTURE_2D, aTexIndex);
        glScalef(0.5, 0.5, 0.5);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        glScalef(2, 2, 2);
        glTranslatef(0, -iScrSize.iHeight+50, 1);
        glScalef(0.5, 0.5, 0.5);
        glBindTexture(GL_TEXTURE_2D, aTexIndex);
        glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        glScalef(2, 2, 2);
        
        // Restore OpenGL state
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_TEXTURE_2D);
        
        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        
        //glDisable(GL_BLEND);
        /*if(aShowLocationRect)
            DrawZoomFrame(iRotationTarget);*/
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glPopMatrix();
        
    }
