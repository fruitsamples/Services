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
#import "MixerController.h"

#import <AudioUnit/AudioUnitParameters.h>
#import <AudioUnit/AudioUnitProperties.h>
#import "MeteringView.h"
#import "CAStreamBasicDescription.h"

#define RequireNoErr(error)	do { if( (error) != noErr ) throw OSStatus(error); } while (false)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____AUComponentDescription

// a convenience wrapper for ComponentDescription

class AUComponentDescription : public ComponentDescription
{
public:
	AUComponentDescription()
	{
		 	componentType = 0;
			componentSubType = 0;
			componentManufacturer = 0;
			componentFlags = 0;
			componentFlagsMask = 0;
	};
	
			
	AUComponentDescription(	OSType			inType,
							OSType			inSubType,
							OSType			inManufacturer = 0,
							unsigned long	inFlags = 0,
							unsigned long	inFlagsMask = 0 )
	{
		 	componentType = inType;
			componentSubType = inSubType;
			componentManufacturer = inManufacturer;
			componentFlags = inFlags;
			componentFlagsMask = inFlagsMask;
	};

	AUComponentDescription(const ComponentDescription &inDescription )
	{
		*(ComponentDescription*)this = inDescription;
	};
};

OSStatus renderInput(void *inRefCon, 
	AudioUnitRenderActionFlags *ioActionFlags, 
	const AudioTimeStamp *inTimeStamp, 
	UInt32 inBusNumber, 
	UInt32 inNumberFrames, 
	AudioBufferList *ioData)
{
    SynthData& d = *(SynthData*)inRefCon; // get access to Sinewave's data
	UInt32 bufSamples = d.bufs[inBusNumber].numFrames << 1;
	float *in = d.bufs[inBusNumber].data;

	float *outA = (float*)ioData->mBuffers[0].mData;
	float *outB = (float*)ioData->mBuffers[1].mData;
	if (!in) {
		for (UInt32 i=0; i<inNumberFrames; ++i) 
		{
			outA[i] = 0.f;
			outB[i] = 0.f;
		}
	} else {
		UInt32 phase = d.bufs[inBusNumber].phase;
		for (UInt32 i=0; i<inNumberFrames; ++i) 
		{
			outA[i] = in[phase++];
			outB[i] = in[phase++];
			if (phase >= bufSamples) phase = 0;
		}
		d.bufs[inBusNumber].phase = phase;
	}
	return noErr;
}

@implementation MixerController

- (void)awakeFromNib
{	
	isPlaying = false;
	automate = false;
	automatePhase = 0;

	memset(&d, 0, sizeof(d));
	[self initializeGraph];
	
	mTimer = [NSTimer scheduledTimerWithTimeInterval: 0.02 
		target: self selector: @selector(doTimer:) userInfo: nil repeats: YES];
		
	[[NSRunLoop currentRunLoop] addTimer: mTimer forMode: NSModalPanelRunLoopMode];
	[[NSRunLoop currentRunLoop] addTimer: mTimer forMode: NSEventTrackingRunLoopMode];
	
	NSView** meter = &xpmeter11;
	for (int i=0, k=0; i<4; ++i) {
		for (int j=0; j<5; ++j, ++k) {
			NSSlider *oldMeter = meter[k];
			NSRect newFrame = [oldMeter frame];
			newFrame.size.width = 11;
			MeteringView* mv = [[MeteringView alloc] initWithFrame: newFrame];
			[mv setNumChannels: 1];
			meter[k] = mv;
			NSView* parent = [oldMeter superview];
			[parent replaceSubview: oldMeter with: mv];
		}
	}
	
	meter = &meterOut0;
	for (int j=0; j<5; ++j) {
		NSSlider *oldMeter = meter[j];
		NSRect newFrame = [oldMeter frame];
		newFrame.size.width = 11;
		MeteringView* mv = [[MeteringView alloc] initWithFrame: newFrame];
		[mv setNumChannels: 1];
		meter[j] = mv;
		NSView* parent = [oldMeter superview];
		[parent replaceSubview: oldMeter with: mv];
	}
	
	meter = &meterIn0;
	for (int j=0; j<4; ++j) {
		NSSlider *oldMeter = meter[j];
		NSRect newFrame = [oldMeter frame];
		newFrame.size.width = 11;
		MeteringView* mv = [[MeteringView alloc] initWithFrame: newFrame];
		[mv setNumChannels: 1];
		meter[j] = mv;
		NSView* parent = [oldMeter superview];
		[parent replaceSubview: oldMeter with: mv];
	}

	meter = &meterInPre0;
	for (int j=0; j<4; ++j) {
		NSSlider *oldMeter = meter[j];
		NSRect newFrame = [oldMeter frame];
		newFrame.size.width = 11;
		MeteringView* mv = [[MeteringView alloc] initWithFrame: newFrame];
		[mv setNumChannels: 1];
		meter[j] = mv;
		NSView* parent = [oldMeter superview];
		[parent replaceSubview: oldMeter with: mv];
	}

}


extern double dbamp(double x);

- (void)doTimer: (NSTimer*) timer
{
	// set meters
	Float32 value;
	OSStatus err;
	MeteringView **meter;
	MeteringView **meterPre;
	NSSlider **slider;
	
	if (automate) {
		automatePhase++;
	}
	
	meter = &meterIn0;
	meterPre = &meterInPre0;
	for (int i=0; i<4; ++i) {
		float amps[2];
		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostAveragePower, kAudioUnitScope_Input, i, &amps[0]);
		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostPeakHoldLevel, kAudioUnitScope_Input, i, &amps[1]);
		value = sqrt(dbamp(value));
		[meter[i] updateMeters: amps];

		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PreAveragePower, kAudioUnitScope_Input, i, &amps[0]);
		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PrePeakHoldLevel, kAudioUnitScope_Input, i, &amps[1]);
		value = sqrt(dbamp(value));
		[meterPre[i] updateMeters: amps];
	}

	meter = &meterOut0;
	for (int i=0; i<5; ++i) {
		float amps[2];
		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostAveragePower, kAudioUnitScope_Output, i, &amps[0]);
		err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostPeakHoldLevel, kAudioUnitScope_Output, i, &amps[1]);
		value = sqrt(dbamp(value));
		[meter[i] updateMeters: amps];
	}

	Float32 phaseinc = ( 2. * 3.14159 ) / 128. ;
	meter = &xpmeter11;
	slider = &xpslider11;
	for (int i=0, k=0; i<4; ++i) {
		for (int j=0; j<5; ++j, ++k) {
			float amps[2];
			UInt32 element = (i<<16) | j;
			err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostAveragePower, kAudioUnitScope_Global, element, &amps[0]);
			err = AudioUnitGetParameter(mixer, kMatrixMixerParam_PostPeakHoldLevel, kAudioUnitScope_Global, element, &amps[1]);
			[meter[k] updateMeters: amps];
			if (automate) {
				Float32 phase = phaseinc * automatePhase * (1.0 + 0.1 * k);
				Float32 volume = 0.5 + 0.5 * sin(phase);
				// this should be ModelViewController, but for demo purposes, cheat..
				[slider[k] setFloatValue: volume * 100.];
				err = AudioUnitSetParameter(mixer, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, volume, 0);
			}
		}
	}
}

- (IBAction)play:(id)sender
{
	if (isPlaying) return;

	printf("PLAY\n");
	OSStatus err = AUGraphStart(mGraph);
	isPlaying = true;
	printf("AUGraphStart %08lX\n", err);
}

- (IBAction)setInputVolume:(id)sender
{
	UInt32 inputNum = [sender tag] / 100 - 1;
	//printf("vin %lu  %g\n", inputNum, [sender doubleValue]);
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Volume, kAudioUnitScope_Input, inputNum, [sender doubleValue] * .01, 0);
}

- (IBAction)setMasterVolume:(id)sender
{
	//printf("master %g\n", [sender doubleValue]);
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Volume, kAudioUnitScope_Global, 0xFFFFFFFF, [sender doubleValue] * .01, 0);
}

- (IBAction)setMatrixVolume:(id)sender
{
	UInt32 inputNum = [sender tag] / 100 - 1;
	UInt32 outputNum = [sender tag] % 100 - 1;
	UInt32 element = (inputNum << 16) | (outputNum & 0x0000FFFF);
	//printf("%lu %lu %08X  %g\n", inputNum, outputNum, element, [sender doubleValue]);
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Volume, kAudioUnitScope_Global, element, [sender doubleValue] * .01, 0);
}

- (IBAction)setOutputVolume:(id)sender
{
	UInt32 outputNum = [sender tag] % 100 - 1;
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Volume, kAudioUnitScope_Output, outputNum, [sender doubleValue] * .01, 0);
}

- (IBAction)stop:(id)sender
{
	if (!isPlaying) return;
	printf("STOP\n");
	OSStatus err = AUGraphStop(mGraph);
	printf("AUGraphStop %08lX\n", err);
	isPlaying = false;
}

- (IBAction)enableInput:(id)sender
{
	UInt32 inputNum = [sender tag] % 1000 - 1;
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Enable, kAudioUnitScope_Input, inputNum, [sender doubleValue], 0);
}

- (IBAction)enableOutput:(id)sender
{
	UInt32 outputNum = [sender tag] % 1000 - 1;
	AudioUnitSetParameter(mixer, kMatrixMixerParam_Enable, kAudioUnitScope_Output, outputNum, [sender doubleValue], 0);
}



- (void)initializeGraph
{

	CAStreamBasicDescription desc;
	
	//printf("initializeGraph\n");
	OSStatus result = noErr;
	
	result = NewAUGraph(&mGraph);
	//printf("NewAUGraph %08X\n", result);

	AUNode outputNode;
	AUNode mixerNode;
	
	AUComponentDescription 	output_desc		(		
												kAudioUnitType_Output,
												kAudioUnitSubType_DefaultOutput,
												kAudioUnitManufacturer_Apple 
												);

	AUComponentDescription 	mixer_desc		(		
												kAudioUnitType_Mixer,
												kAudioUnitSubType_MatrixMixer,
												kAudioUnitManufacturer_Apple 
												);

	result = AUGraphNewNode(mGraph, &output_desc, 0, NULL, &outputNode);
	if (result) {
		printf("AUGraphNewNode 1 result %lu %4.4s\n", result, (char*)&result);
		return;
	}

	result = AUGraphNewNode(mGraph, &mixer_desc, 0, NULL, &mixerNode );
	if (result) {
		printf("AUGraphNewNode 2 result %lu %4.4s\n", result, (char*)&result);
		return;
	}

	result = AUGraphConnectNodeInput(mGraph, mixerNode, 0, outputNode, 0 );
	if (result) {
		printf("AUGraphConnectNodeInput result %lu %4.4s\n", result, (char*)&result);
		return;
	}
	
	result = AUGraphOpen(mGraph);
	if (result) {
		printf("AUGraphOpen result %lu %4.4s\n", result, (char*)&result);
		return;
	}
	
	result = AUGraphGetNodeInfo(mGraph, mixerNode, NULL, NULL, NULL, &mixer  );
	
	UInt32 size;
	UInt32 data = 1;

	// turn metering ON
	result = AudioUnitSetProperty(	mixer,
							kAudioUnitProperty_MeteringMode,
							kAudioUnitScope_Global,
							0,
							&data,
							sizeof(data) );	
													
	UInt32 numbuses;
	size = sizeof(numbuses);
	
	// set bus counts
	numbuses = 2;
	result = AudioUnitSetProperty(	mixer,
							kAudioUnitProperty_BusCount,
							kAudioUnitScope_Input,
							0,
							&numbuses,
							sizeof(UInt32) );
	
	numbuses = 1;
	result = AudioUnitSetProperty(	mixer,
							kAudioUnitProperty_BusCount,
							kAudioUnitScope_Output,
							0,
							&numbuses,
							sizeof(UInt32) );
	
	for (int i=0; i<2; ++i) {
		// set render callback
		AURenderCallbackStruct rcbs;
		rcbs.inputProc = &renderInput;
		rcbs.inputProcRefCon = &d;
		result = AudioUnitSetProperty(	mixer,
								kAudioUnitProperty_SetRenderCallback,
								kAudioUnitScope_Input,
								i,
								&rcbs,
								sizeof(rcbs) );
								
		// set input stream format
		size = sizeof(desc);
		result = AudioUnitGetProperty(	mixer,
								kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Input,
								i,
								&desc,
								&size );
		
		desc.ChangeNumberChannels(2, false);						
	
		result = AudioUnitSetProperty(	mixer,
								kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Input,
								i,
								&desc,
								sizeof(desc) );
	}
	
								
	// set output stream format
	result = AudioUnitGetProperty(	mixer,
							kAudioUnitProperty_StreamFormat,
							kAudioUnitScope_Output,
							0,
							&desc,
							&size );
	
	desc.ChangeNumberChannels(5, false);						

	result = AudioUnitSetProperty(	mixer,
							kAudioUnitProperty_StreamFormat,
							kAudioUnitScope_Output,
							0,
							&desc,
							sizeof(desc) );

								
// NOW that we've set everything up we can initialize the graph 
// (which will also validate the connections)
	RequireNoErr(AUGraphInitialize(mGraph));

}


- (IBAction)addFile:(id)sender
{
	[self stop: nil];
	
    int result;
    NSArray *fileTypes = [NSArray arrayWithObjects: @"AIFF", @"aif", @"aiff", @"aifc", @"wav", @"WAV", 0];
    NSOpenPanel *oPanel = [NSOpenPanel openPanel];

    [oPanel setAllowsMultipleSelection:YES];
    result = [oPanel runModalForDirectory:NSHomeDirectory()
                    file:nil types:fileTypes];
    if (result == NSOKButton) 
	{
        NSArray *filesToOpen = [oPanel filenames];
        int i, count = [filesToOpen count];
		for (i=0; i<MAXBUFS; ++i) 
		{
			if (d.bufs[i].data) 
			{
				free(d.bufs[i].data);
				d.bufs[i].data = 0;
			}
			if (d.bufs[i].name) 
			{
				[d.bufs[i].name release];
				d.bufs[i].name = 0;
			}
		}
		
		CFAllocatorRef alloc = CFAllocatorGetDefault();

        for (i=0; i<count && i<MAXBUFS; i++) 
		{
            NSString *aFile = [filesToOpen objectAtIndex: i];
			NSString *name = [aFile lastPathComponent];
			
			CFURLRef url = CFURLCreateWithFileSystemPath(alloc, (CFStringRef)aFile, kCFURLPOSIXPathStyle, true);
			if (!url) return;
		
			FSRef fsref;
			Boolean res = CFURLGetFSRef(url, &fsref);
			if (!res) return;

			AudioFileID afid;
			OSStatus err = AudioFileOpen(&fsref, fsRdPerm, 0, &afid);
			
			if (err || !afid) continue;
			
			UInt64 numFrames = 0;
			UInt32 propSize = sizeof(UInt64);
			err = AudioFileGetProperty(afid, kAudioFilePropertyAudioDataPacketCount, &propSize, &numFrames);
			if (err) continue;
			
			d.bufs[i].numFrames = numFrames;

			propSize = sizeof(AudioStreamBasicDescription);
			err = AudioFileGetProperty(afid, kAudioFilePropertyDataFormat, &propSize, &d.bufs[i].asbd);
			
			if (d.bufs[i].asbd.mBitsPerChannel != 32 
					|| !(d.bufs[i].asbd.mFormatFlags & kAudioFormatFlagIsFloat)) 
			{
				continue;
			}
			
			d.bufs[i].name = name;
			[d.bufs[i].name retain];
			
			int samples = numFrames * d.bufs[i].asbd.mChannelsPerFrame;
			d.bufs[i].data = (float*)calloc(samples, sizeof(Float32));
			d.bufs[i].phase = 0;
			
			UInt32 numPackets = numFrames;
			err = AudioFileReadPackets(afid, false, NULL, NULL, 0, &numPackets, d.bufs[i].data);
			if (err) {
				free(d.bufs[i].data);
				d.bufs[i].data = 0;
				continue;
			}
			
			AudioFileClose(afid);
        }
		d.numbufs = count;
    }	
}

- (IBAction)automateOn:(id)sender;
{
	automate = true;
}

- (IBAction)automateOff:(id)sender;
{
	automate = false;
}

@end
