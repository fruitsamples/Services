/*	Copyright: 	© Copyright 2003 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	AUMixer3DView.cpp
	
=============================================================================*/

#include "AUCarbonViewBase.h"

#include <math.h>

class AUMixer3DView : public AUCarbonViewBase {
public:
	AUMixer3DView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv) { }
	
	virtual ComponentResult		GetViewSize(Point &outSize)
	{
		outSize.h = 640;
		outSize.v = 480;
		return noErr;
	}

	AudioUnit					GetAudioUnit() {return mEditAudioUnit;};

	
	virtual OSStatus			CreateCarbonControls();

	WindowRef			GetMacWindow() {return mCarbonWindow;};

};

COMPONENT_ENTRY(AUMixer3DView)


class TrackingLine
{
public:
	TrackingLine(const RGBColor &inColor, int inWidth, int inHeight )
		: mColor(inColor), width(inWidth), height(inHeight)
	{
		centerx = 0.5*width;
		centery = 0.5*height;
		
		lastAngle = 0.0;
		CartesianFromAngle(lastAngle, lastx, lasty );
	}
	
	void			Track(float angle )
	{
		int x,y;
		CartesianFromAngle(angle, x, y );

		RGBColor color = {65535,65535,65535};
		RGBForeColor(&color);
		
		MoveTo(centerx, centery);
		LineTo(lastx,lasty);
		
		lastx = x;
		lasty = y;
		lastAngle = angle;

		Draw();
	};
	
	void			Draw()
	{
		RGBForeColor(&mColor);
		MoveTo(centerx, centery);
		LineTo(lastx,lasty);
	}
	
	// returns distance between this point and our last point
	float			Distance(float inAngle )
	{
		return fabs( inAngle - lastAngle );
	};



private:
	void			CartesianFromAngle(float inAngle, int &outX, int &outY )
	{
		float rangle = 3.14159 * inAngle/180.0;
		outX = centerx + sin(rangle) * 150.0;
		outY = centery - cos(rangle) * 150.0;
	};
	
	RGBColor		mColor;

	int				width;
	int				height;
	int				lastx;
	int				lasty;
	
	int				centerx;
	int				centery;
	
	float			lastAngle;
};

TrackingLine *gTrackers[2] = {NULL,NULL};


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	MyEventHandler
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
static pascal OSStatus MyEventHandler(EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
{
	UInt32 eventClass = GetEventClass(inEvent);
	UInt32 eventKind = GetEventKind(inEvent);
	
	AUMixer3DView *This = (AUMixer3DView*)inUserData;
	
	if(eventClass == kEventClassMouse )
	{
		EventParamName 	paramName;
		HIPoint			mousePos;
		OSStatus result =  GetEventParameter(  	inEvent,
												kEventParamMouseLocation,
												typeHIPoint,
												NULL,       /* can be NULL */
												sizeof(HIPoint),
												NULL,       /* can be NULL */
												&mousePos);
												
		CGrafPtr gp = GetWindowPort(This->GetMacWindow());
		
		

		Rect r2 = GetPortBitMapForCopyBits(gp)->bounds;
		
		Point pt;
		pt.h = mousePos.x;
		pt.v = mousePos.y;
		
		
		CGrafPtr save;
		GetPort(&save);
		SetPort(gp );
		GlobalToLocal(&pt);

		// check for title bar
		if(pt.v < 0)
		{
			return eventNotHandledErr;
		}


		Rect r1;
		GetPortBounds(gp, &r1 );
		int width = r1.right-r1.left;
		int height = r1.bottom-r1.top;

		int centerx = 0.5*width;
		int centery = 0.5*height;
		
		static int currentLine = 0;
		
		float x = pt.h - centerx;
		float y = -(pt.v - centery);
		
		float rangle = atan2(x,y);
		float angle = 180.0 * rangle / 3.14159;
		



		switch(eventKind)
		{
			case kEventMouseDown:
				if(gTrackers[0]->Distance(angle) < gTrackers[1]->Distance(angle) )
				{
					currentLine = 0;
				}
				else
				{
					currentLine = 1;
				}
				
			case kEventMouseDragged:
			{
				TrackingLine *trackingLine = gTrackers[currentLine];
				
				if(trackingLine)
				{
					trackingLine->Track(angle);
					gTrackers[1-currentLine]->Draw();	// in case we overwrote the other line
				}
				
				AudioUnitSetParameter(This->GetAudioUnit(),
					0 /* azimuth */,
					kAudioUnitScope_Input,
					currentLine,
					angle,
					0 );
				
				float distance = 0.1 * sqrt( x*x + y*y);

				AudioUnitSetParameter(This->GetAudioUnit(),
					2 /* distance */,
					kAudioUnitScope_Input,
					currentLine,
					distance,
					0 );
				
				break;
			}
		
			case kEventMouseUp:
				break;
		
		}




		SetPort(save);

		return noErr;
	}
	else
	{
		EventParamName 	paramName;
		UInt8			keyCode;
		OSStatus result =  GetEventParameter(  	inEvent,
												kEventParamKeyMacCharCodes,
												typeChar,
												NULL,       /* can be NULL */
												sizeof(UInt8),
												NULL,       /* can be NULL */
												&keyCode);
	
	
		printf("keycode = %c\n", keyCode );
		
		return noErr;
	}
	
	return eventNotHandledErr;
}





//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUMixer3DView::CreateCarbonControls
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	AUMixer3DView::CreateCarbonControls()
{
	EventHandlerUPP  handlerUPP = NewEventHandlerUPP(MyEventHandler);

	EventTypeSpec    eventTypes[5];
	eventTypes[0].eventClass = kEventClassKeyboard;
	eventTypes[0].eventKind  = kEventRawKeyDown;

	eventTypes[1].eventClass = kEventClassKeyboard;
	eventTypes[1].eventKind  = kEventRawKeyRepeat;

	eventTypes[2].eventClass = kEventClassMouse;
	eventTypes[2].eventKind  = kEventMouseDown;

	eventTypes[3].eventClass = kEventClassMouse;
	eventTypes[3].eventKind  = kEventMouseUp;

	eventTypes[4].eventClass = kEventClassMouse;
	eventTypes[4].eventKind  = kEventMouseDragged;


	SizeControl(mCarbonPane, 400, 400);

	InstallWindowEventHandler( GetMacWindow(), handlerUPP, 5, eventTypes, this, NULL);


	CGrafPtr gp = GetWindowPort(GetMacWindow());
	Rect r1;
	GetPortBounds(gp, &r1 );
	int width = r1.right-r1.left;
	int height = r1.bottom-r1.top;

	RGBColor color1;
	color1.red = 65535;
	color1.green = 0;
	color1.blue = 0;
	gTrackers[0] = new TrackingLine(color1, 400, 400);

	RGBColor color2;
	color2.red = 0;
	color2.green = 65535;
	color2.blue = 0;
	gTrackers[1]  = new TrackingLine(color2, 400, 400);


#if 0
	CGrafPtr save;
	GetPort(&save);
	SetPort(gp );
	
	gTrackers[0]->Draw();
	gTrackers[1]->Draw();
	
	SetPort(save );
#endif//0
	
	return noErr;
}

