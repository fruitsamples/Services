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
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// main.cpp
//
#include <CoreServices/CoreServices.h>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include <unistd.h> //for usleep

#include "AudioFilePlay.h"

const char* theAudioFile = "/System/Library/Sounds/Submarine.aiff";
Boolean playbackIsFinished = FALSE;

OSStatus MatchAUFormats (AudioUnit theUnit, UInt32 theInputBus);
							
#define RETURN_RESULT(str) 										\
	if (result) {												\
		printf ("Error:%s=0x%lX,%ld,%s\n\n",		 			\
						str,result, result, (char*)&result);	\
		fflush (stdout);										\
        return result;											\
	}

// this is the default behaviour, but we'll do this anyway	
void FilePlayNotificationHandler (void *				inRefCon,
							OSStatus					inStatus)
{
		// if we receive any notification we'll just disconnect ourselves
        // and flag that we should quit.
	printf ("FilePlay notification:%ld\n", inStatus);
	AFP_Disconnect ((AudioFilePlayID)inRefCon);
	AFP_Print ((AudioFilePlayID)inRefCon);
    playbackIsFinished = TRUE;
}

int main(int argc, char* argv[])
{
	OSStatus result = noErr;
	
	const char* theFile = theAudioFile;
	if (argc > 1) theFile = argv[1];
    
	FSRef ref;
	result = FSPathMakeRef ((const UInt8 *)theFile, &ref, NULL);
		RETURN_RESULT("FSPathMakeRef")
    
    AudioUnit theUnit;
    ComponentDescription desc;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    
    Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) return internalComponentErr;
    
    result = OpenAComponent(comp, &theUnit);
        RETURN_RESULT("OpenAComponent")
                
        // you need to initialize the output unit before you set it as a destination
    result = AudioUnitInitialize (theUnit);
        RETURN_RESULT("AudioUnitInitialize")
    
    AudioFilePlayID player;
    result = NewAudioFilePlayID (&ref, &player);
        RETURN_RESULT("NewAudioFilePlayID")
        
// In this case we first want to get the output format of the OutputUnit
// Then we set that as the input format. Why?
// So that only a single conversion process is done
// when SetDestination is called it will get the input format of the
// unit its supplying data to. This defaults to 44.1K, stereo, so if
// the device is not that, then we lose a possibly rendering of data
    
    result = MatchAUFormats (theUnit, 0);
        RETURN_RESULT ("SetAUFormat")
        
    result = AFP_SetDestination (player, theUnit, 0);
        RETURN_RESULT("AFP_SetDestination")
        
    result = AFP_SetNotifier (player, FilePlayNotificationHandler, player);
        RETURN_RESULT("AFP_SetNotifier")
        
    result = AFP_Connect (player);
        RETURN_RESULT("AFP_Connect")

    AFP_Print (player);
    fflush (stdout);
    
    result = AudioOutputUnitStart (theUnit);
        RETURN_RESULT("AudioOutputUnitStart")
    
    // check every 1/4 second to see if playback is done
    while (!playbackIsFinished) {
        usleep (250000);
    }
    
    result = AudioOutputUnitStop (theUnit);
        RETURN_RESULT("AudioOutputUnitStop")

    result = AFP_Disconnect (player);
        RETURN_RESULT("AFP_Disconnect")

    result = DisposeAudioFilePlayID (player);
        RETURN_RESULT("DisposeAudioFilePlayID")
        
	return 0;
}

OSStatus MatchAUFormats (AudioUnit theUnit, UInt32 theInputBus)
{
	AudioStreamBasicDescription theDesc;
	UInt32 size = sizeof (theDesc);
	OSStatus result = AudioUnitGetProperty (theUnit,
							kAudioUnitProperty_StreamFormat,
							kAudioUnitScope_Output,
							0,
							&theDesc,
							&size);
		RETURN_RESULT("AudioUnitGetProperty")

	result = AudioUnitSetProperty (theUnit,
							kAudioUnitProperty_StreamFormat,
							kAudioUnitScope_Input,
							theInputBus,
							&theDesc,
							size);
	
	return result;
}
