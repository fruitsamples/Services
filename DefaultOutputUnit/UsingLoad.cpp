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
/*
        File:			UsingDefaultNoAC.cpp
		
		Description:	DefaultOutputUnit
						
						This is a command line tool that generates a sine wave at the specified frequency
						
						It uses the DefaultOutputUnit, so it will continue to generate this sine wave:
						- if the user changes the destination audio device for the default output
						- if that device has a different stream format than the previous one
						
        Author:			Doug Wyatt, William Stewart
*/

#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/DefaultAudioOutput.h>

#include <math.h>

extern UInt32 theFormatID;

UInt32 kFramesPerSlice = 128;
UInt64 kLoadUpTime;
static double kLoadFactor = 0.7;
double kDeviceSampleRate = 44100.0;
UInt64	startRunningTime;


// THESE values can be read from your data source
// they're used to tell the DefaultOutputUnit what you're giving it


static const double	kAmplitude = 0.25;
static const double	kToneFrequency = 440.;
static const SInt32	kNumChannels = 2;
bool doRender = true;

UInt32			gSinWaveFrameCount = 0; // this keeps track of the number of frames you render


OSStatus OverlaodListenerProc(	AudioDeviceID			inDevice,
								UInt32					inChannel,
								Boolean					isInput,
								AudioDevicePropertyID	inPropertyID,
								void*					inClientData)
{
	printf ("* * * * * Overload detected on device playing audio\n");
	UInt64 overloadTime = AudioGetCurrentHostTime();
	overloadTime = AudioConvertHostTimeToNanos (overloadTime - startRunningTime);
	printf ("\tSeconds after start = %lf\n", double(overloadTime / 1000000000.));
	return noErr;
}



void CalculateLoadFactor ()
{
	double kTotalRenderTime = (kFramesPerSlice / kDeviceSampleRate * 1000.0) * 1000 * 1000; //msecs per slice -> nanos
	double kLoadTime = kTotalRenderTime * kLoadFactor;
	kLoadUpTime = AudioConvertNanosToHostTime (UInt64(kLoadTime));
	printf ("running at %f load\n", kLoadFactor);
}

// simulates reading the UInt data from the disk
// returns the adjusted frame count
void RenderSin (UInt32 startingFrameCount, UInt32 nFrames, void* dataPtr, UInt32 sizeOfData) {
	Float32 *fp = (Float32 *)dataPtr;

	double j = startingFrameCount;
	double cycleLength = kDeviceSampleRate / kToneFrequency;
	for (UInt32 i = 0; i < nFrames; ++i) {
		// generate nFrames 32-bit floats
		Float32 nextFloat = sin(j / cycleLength * (M_PI * 2.0)) * kAmplitude;
		for (int i = 0; i < kNumChannels; ++i)
			fp[i] = nextFloat;
		
		fp += kNumChannels;

		j += 1.0;
		if (j > cycleLength)
			j -= cycleLength;
	}
}

OSStatus	MyRenderer(void *inRefCon, AudioUnitRenderActionFlags inActionFlags, 
	const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, 
	AudioBuffer *ioData)
{
	if (!doRender) {
		memset (ioData->mData, 0, ioData->mDataByteSize);
		return noErr;
	}

	UInt64 now = AudioGetCurrentHostTime();
	
	UInt32 nFrames = ioData->mDataByteSize / (sizeof(Float32) * ioData->mNumberChannels);
	RenderSin (gSinWaveFrameCount, nFrames,  ioData->mData, ioData->mDataByteSize);
	gSinWaveFrameCount += nFrames;
	
	UInt64 loop = now + kLoadUpTime;
	while (AudioGetCurrentHostTime() < loop)
		;
	
	return noErr;
}

// ________________________________________________________________________________
//
// TestDefaultAU
//
void	TestDefaultAU()
{
	AudioUnit theOutputUnit;
	OSStatus err;

	ComponentDescription desc;

	desc.componentType = 'aunt';
	desc.componentSubType = kAudioUnitSubType_Output;
	desc.componentManufacturer = kAudioUnitID_DefaultOutput;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
		
	Component comp = FindNextComponent(NULL, &desc);
		if (comp == NULL) exit (-1);
		
	err = OpenAComponent(comp, &theOutputUnit);
		if (err)  exit (-1);

	// Initialize it
	verify_noerr(AudioUnitInitialize(theOutputUnit));
	
	// Set up a callback function to generate output to the output unit
	AudioUnitInputCallback input;
	input.inputProc = MyRenderer;
	input.inputProcRefCon = NULL;
	
	verify_noerr(AudioUnitSetProperty(theOutputUnit, 
								kAudioUnitProperty_SetInputCallback, 
								kAudioUnitScope_Global,
								0,
								&input, 
								sizeof(input)));

	// We tell the Output Unit what format we're going to supply data to it
	// this is necessary if you're providing data through an input callback
	// AND you want the DefaultOutputUnit to do any format conversions
	// necessary from your format to the device's format.
// data is 32 bit float
	AudioStreamBasicDescription streamFormat;
		streamFormat.mSampleRate = kDeviceSampleRate;		//	the sample rate of the audio stream
		streamFormat.mFormatID = theFormatID;			//	the specific encoding type of audio stream
		streamFormat.mFormatFlags = kLinearPCMFormatFlagIsFloat 
								| kLinearPCMFormatFlagIsBigEndian
								| kLinearPCMFormatFlagIsPacked;		//	flags specific to each format
		streamFormat.mBytesPerPacket = kNumChannels * sizeof (Float32);	
		streamFormat.mFramesPerPacket = 1;	
		streamFormat.mBytesPerFrame = kNumChannels * sizeof (Float32);		
		streamFormat.mChannelsPerFrame = kNumChannels;	
		streamFormat.mBitsPerChannel = sizeof (Float32) * 8;	
	
	verify_noerr(AudioUnitSetProperty(
		theOutputUnit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Input,
		0,
		&streamFormat,
		sizeof(AudioStreamBasicDescription)));
	
	printf("Rendering source:\n\t");
	printf ("SampleRate=%f,", streamFormat.mSampleRate);
	printf ("BytesPerPacket=%ld,", streamFormat.mBytesPerPacket);
	printf ("FramesPerPacket=%ld,", streamFormat.mFramesPerPacket);
	printf ("BytesPerFrame=%ld,", streamFormat.mBytesPerFrame);
	printf ("BitsPerChannel=%ld,", streamFormat.mBitsPerChannel);
	printf ("ChannelsPerFrame=%ld\n", streamFormat.mChannelsPerFrame);
	
	AudioDeviceID theDevice;
	UInt32 theSize = sizeof (theDevice);
	verify_noerr(AudioUnitGetProperty (theOutputUnit, kAudioOutputUnitProperty_CurrentDevice, 0, 0, &theDevice, &theSize));
	
	theSize = sizeof (kFramesPerSlice);
	verify_noerr(AudioDeviceSetProperty(theDevice, 0, 0, false, kAudioDevicePropertyBufferFrameSize, theSize, &kFramesPerSlice));

	theSize = sizeof (kFramesPerSlice);
	verify_noerr(AudioDeviceGetProperty(theDevice, 0, false, kAudioDevicePropertyBufferFrameSize, &theSize, &kFramesPerSlice));

	verify_noerr (AudioDeviceAddPropertyListener(theDevice, 0, false,
						kAudioDeviceProcessorOverload, OverlaodListenerProc, 0));

	printf ("size of the device's buffer = %ld frames\n", kFramesPerSlice);
	
	CalculateLoadFactor();

	// Start the rendering
	// The DefaultOutputUnit will do any format conversions to the format of the default device
	startRunningTime = AudioGetCurrentHostTime ();

	verify_noerr(AudioOutputUnitStart(theOutputUnit));
	
	while (true) {
		usleep (1000000);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
		verify_noerr(AudioOutputUnitStop(theOutputUnit));
		usleep (50000);
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
		verify_noerr(AudioOutputUnitStart(theOutputUnit));
	}	
		
	bool run = true;
	char ch;
	while(run) {
		ch = getc(stdin);
		switch (ch) {
			case 'q': case 'Q':
				run = false;
				break;
			case 'n': case 'N':
				doRender = false;
				break;
			case 'y': case 'Y':
				doRender = true;
				break;
			case 'l': case 'L':
			{
				char chars[10];
				int i = 0;
				while ((ch = getc(stdin)) != '\n')
					chars[i++] = ch;
				chars[i] = 0;
				kLoadFactor = atof (chars);
				CalculateLoadFactor();
			}
			break;
		}
	}

// REALLY after you're finished playing STOP THE AUDIO OUTPUT UNIT!!!!!!	
// but we never get here because we're running until the process is nuked...	
	verify_noerr(AudioOutputUnitStop(theOutputUnit));
	
	// Clean up
	CloseComponent(theOutputUnit);
}

// ________________________________________________________________________________
//
// TestDefaultAU
//
int main(int argc, const char * argv[])
{
	printf ("in main\n");
	if (argc > 1)
	{
		kLoadFactor = atof (argv[1]);
		
		if (argc > 2)
			kFramesPerSlice = atoi (argv[2]);
			
		if (argc > 3)
			kDeviceSampleRate = atof (argv[3]);
	}
		
	TestDefaultAU();

    return 0;
}
