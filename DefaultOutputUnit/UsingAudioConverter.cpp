/*	Copyright: 	© Copyright 2004 Apple Computer, Inc. All rights reserved.

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
        File:			UsingAudioConverter.cpp
		
		Description:	DefaultOutputUnit
						
						This is a command line tool that generates a sine wave at the specified frequency
						
						It supplies data to the output side of the DefaultOutputUnit
						- that way you get REGULAR calls from the unit if you're using this to generate timing information
						- it uses the AudioConverter to convert the source format to the device
						- you MUST register for StreamFormat notifications, so if the device is changed you'll be able to reset your conversion
						
        Author:			Doug Wyatt, William Stewart
*/

#include <CoreServices/CoreServices.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/DefaultAudioOutput.h>
#include <AudioToolbox/AudioConverter.h>

#include <math.h>

// These defines will alter the runtime behaviour of this program
#define SOURCE_AS_FLOAT 1
#define DO_PRINT 1

static UInt32					gOutputFrameCount = 0; // this keeps track of the number of frames you render to the device - you could use this count to derive a sample-based clock

// THESE values can be read from your data source
// they're used to tell the DefaultOutputUnit what you're giving it
static const Float64			gSourceSampleRate = 48000;//48000.0;// we could -> 96000. !!
static Float64					gOutputSampleRate;

// sin wave consts
// used to calculate size of buffer for sin waves...
static const UInt32 			numInputFrames = 128;

static const UInt32 			theFormatID = kAudioFormatLinearPCM;

static AudioUnit theOutputUnit;

#if SOURCE_AS_FLOAT
// data is 32 bit float
	UInt32 theFormatFlags =  kLinearPCMFormatFlagIsFloat 
								| kLinearPCMFormatFlagIsBigEndian
								| kLinearPCMFormatFlagIsPacked;
	UInt32 numBytesInAPacket = 8;
	UInt32 numFramesPerPacket = 1; // this shouldn't change
	UInt32 numBytesPerFrame = 8; //
	UInt32 numChannelsPerFrame = 2;
	UInt32 numBitsPerChannel = 32;

	Float32 sourceData[numInputFrames * 2];
#else
// data is 16bit signed integer	
	UInt32 theFormatFlags =  kLinearPCMFormatFlagIsSignedInteger 
								| kLinearPCMFormatFlagIsBigEndian
								| kLinearPCMFormatFlagIsPacked;
	UInt32 numBytesInAPacket = 4;
	UInt32 numFramesPerPacket = 1; // this shouldn't change
	UInt32 numBytesPerFrame = 4; //
	UInt32 numChannelsPerFrame = 2;
	UInt32 numBitsPerChannel = 16;

	SInt16 sourceData[numInputFrames * 2];
#endif

static UInt32 					sinWaveFrameCount = 0;

static const double	kAmplitude = 0.25;
static const double	kToneFrequency = 440.;

// simulates reading the UInt data from the disk
// returns the adjusted frame count
void RenderSin (UInt32 startingFrameCount, UInt32 nFrames, void* dataPtr) {
#if SOURCE_AS_FLOAT
	Float32 *fp = (Float32 *)dataPtr;
#else
	SInt16 *ip = (SInt16 *)dataPtr;
#endif

	double j = startingFrameCount;
	double cycleLength = gSourceSampleRate / kToneFrequency;
	for (UInt32 i = 0; i < nFrames; ++i) {
		// generate nFrames 32-bit floats
		Float32 nextFloat = sin(j / cycleLength * (M_PI * 2.0)) * kAmplitude;
#if SOURCE_AS_FLOAT
		fp[0] = fp[1] = nextFloat;
		fp += 2;
#else
		ip[0] = ip[1] = (SInt16)(nextFloat * 32767.);
		ip += 2;
#endif
		j += 1.0;
		if (j > cycleLength)
			j -= cycleLength;
	}
}

#if DO_PRINT
	UInt32 lastSampleCount = 0;
	double secTimeIntervalToPrint = 1.0;
	double lastPrintTime = 0;
	double nextPrintTime = 0;
	double numSecsBetweenPrints = 1;
	static UInt32 					lastSinWaveFrameCount = 0;
#endif

AudioConverterRef converter;

AudioStreamBasicDescription sourceStreamFormat, destStreamFormat;

// THis is the proc that supplies the data to the AudioConverterFillBuffer call
// The FillBuffer call is used to fill the buffer from the OutputUnit's render slice call
OSStatus MyACInputProc (AudioConverterRef			inAudioConverter,
								UInt32*						outDataSize,
								void**						outData,
								void*						inUserData)
{
	RenderSin (sinWaveFrameCount, numInputFrames, sourceData);
	sinWaveFrameCount += numInputFrames;
	*outData = &sourceData;
#if SOURCE_AS_FLOAT
	*outDataSize = numInputFrames * 2 * 4;
#else
	*outDataSize = numInputFrames * 2 * 2;
#endif
	return noErr;
}

OSStatus	MyRenderer(void *inRefCon, AudioUnitRenderActionFlags inActionFlags, 
	const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, 
	AudioBuffer *ioData)
{
	UInt32 size = ioData->mDataByteSize;
	verify_noerr (AudioConverterFillBuffer(
			converter,
			MyACInputProc,
			0,
			&size,
			ioData->mData));
	
	// we count sample at the hardware output buffer size/sample rate
	// we DON'T care how many samples we consume on input to fill that buffer
	// as this will work by implication...
	UInt32 nFrames = ioData->mDataByteSize / (sizeof(Float32) * ioData->mNumberChannels);
	gOutputFrameCount += nFrames;
	
#if DO_PRINT	
	if (gOutputFrameCount / gOutputSampleRate > nextPrintTime) {
		printf ("OUTPUT: totalProcessedFrames=%ld, sampleCountForThisSlice=%f, timeSinceStarted=%fsecs,",
							gOutputFrameCount, inTimeStamp->mSampleTime, (gOutputFrameCount / gOutputSampleRate));
		printf ("samplesThisSlice=%ld, timeThisSlice=%fsecs\n",
			(gOutputFrameCount - lastSampleCount), (gOutputFrameCount / gOutputSampleRate - lastPrintTime));
		printf ("\tINPUT: totalRenderedSinWaveFrames=%ld, framesRenderedThisSlice=%ld\n", sinWaveFrameCount, (sinWaveFrameCount - lastSinWaveFrameCount));
		
		lastPrintTime = gOutputFrameCount / gOutputSampleRate;
		nextPrintTime = lastPrintTime + numSecsBetweenPrints;
		lastSampleCount = gOutputFrameCount;
		lastSinWaveFrameCount = sinWaveFrameCount;
	}
#endif	

	return noErr;
}


void MyStreamPropertyListener (void *inRefCon, 
						AudioUnit ci, 
						AudioUnitPropertyID inID, 
						AudioUnitScope inScope, 
						AudioUnitElement inElement)
{
	// we're only registered for Stream properties so just check that scope is output
	if (inScope == kAudioUnitScope_Output) {
	// OK the streamFormat is now changed - set up the converter
		verify_noerr(AudioOutputUnitStop(theOutputUnit));
	
		UInt32 aStreamSize = sizeof(AudioStreamBasicDescription);
		verify_noerr(AudioUnitGetProperty(
			theOutputUnit,
			kAudioUnitProperty_StreamFormat,
			kAudioUnitScope_Output,
			0,
			&destStreamFormat,
			&aStreamSize));
		
		gOutputSampleRate = destStreamFormat.mSampleRate;
#if DO_PRINT
	lastPrintTime = 0;
	nextPrintTime = 0;
#endif
		printf ("New Destination Format: newSampleRate=%f\n", destStreamFormat.mSampleRate);
		verify_noerr(AudioConverterDispose (converter));
		
		verify_noerr(AudioConverterNew (
			&sourceStreamFormat, 
			&destStreamFormat, 
			&converter));

		verify_noerr(AudioOutputUnitStart(theOutputUnit));
	}
}

// ________________________________________________________________________________
//
// TestDefaultAU
//
void	TestDefaultAU()
{
	OSStatus err;

	// Open the default output unit
	// (can also use OpenSystemSoundAudioOutput for alert sounds, modem tones, etc.)
	verify_noerr(err = OpenDefaultAudioOutput(&theOutputUnit));
	if (err) return;

	// Initialize it
	verify_noerr(AudioUnitInitialize(theOutputUnit));
	
	// Set up a callback function to generate output to the output unit
	AudioUnitInputCallback input;
	input.inputProc = MyRenderer;
	input.inputProcRefCon = NULL;
	
	verify_noerr(AudioUnitSetProperty(theOutputUnit, 
								kAudioUnitProperty_SetInputCallback, 
								kAudioUnitScope_Input,
								0,
								&input, 
								sizeof(input)));

	// We tell the Output Unit what format we're going to supply data to it
	// this is necessary if you're providing data through an input callback
	// AND you want the DefaultOutputUnit to do any format conversions
	// necessary from your format to the device's format.
	UInt32 aStreamSize = sizeof(AudioStreamBasicDescription);
	
	verify_noerr(AudioUnitGetProperty(
		theOutputUnit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Output,
		0,
		&destStreamFormat,
		&aStreamSize));
	
	gOutputSampleRate = destStreamFormat.mSampleRate;
	
	verify_noerr(AudioConverterNew (
		&sourceStreamFormat, 
		&destStreamFormat, 
		&converter));
	
	printf("Rendering source from %f to %f\n", sourceStreamFormat.mSampleRate, destStreamFormat.mSampleRate);

 	verify_noerr(AudioUnitAddPropertyListener(
		theOutputUnit,
		kAudioUnitProperty_StreamFormat,
		MyStreamPropertyListener,
		0));

 
  
	// Start the rendering
	// The DefaultOutputUnit will do any format conversions to the format of the default device
	verify_noerr(AudioOutputUnitStart(theOutputUnit));
			
	while (1) {
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, false);
		usleep (3000000);
	}

	printf ("done\n");

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
	sourceStreamFormat.mSampleRate = gSourceSampleRate;		//	the sample rate of the audio stream
	sourceStreamFormat.mFormatID = theFormatID;			//	the specific encoding type of audio stream
	sourceStreamFormat.mFormatFlags = theFormatFlags;		//	flags specific to each format
	sourceStreamFormat.mBytesPerPacket = numBytesInAPacket;	
	sourceStreamFormat.mFramesPerPacket = numFramesPerPacket;	
	sourceStreamFormat.mBytesPerFrame = numBytesPerFrame;		
	sourceStreamFormat.mChannelsPerFrame = numChannelsPerFrame;	
	sourceStreamFormat.mBitsPerChannel = numBitsPerChannel;	

	TestDefaultAU();

    return 0;
}
