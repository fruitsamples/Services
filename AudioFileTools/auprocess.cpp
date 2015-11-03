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
/*=============================================================================
	main.cpp
	
=============================================================================*/

/*
	auprocess
		- takes a source audio file, an AU and generates a processed file 
*/

#include "CAAUProcessor.h"
#include "CAAudioFile.h"
#include "CAXException.h"
#include "CAHostTimeBase.h"

#if CAAUDIOFILE_PROFILE
			UInt64 sReadTime = 0;
			UInt64 sRenderTime = 0;
#endif

#pragma mark __print helpers

void PRINT_MARKS ()
{
	printf ("| ");
	for (int i = 0; i < 48; ++i)
		printf (" ");
	printf ("|\n");
}

void PerfResult(const char *toolname, int group, const char *testname, double value, const char *units, const char *fmt="%.3f")
{
	printf("<result tool='%s' group='%d' test='%s' value='", toolname, group, testname);
	printf(fmt, value);
	printf("' units='%s' />\n", units);
}

static int lastProgressPrintDone = -1;
void PRINT_PROGRESS (Float32 inPercent) 
{
	int current = int(inPercent / 4.0);
	for (int i = lastProgressPrintDone; i < current; ++i)
		printf ("* ");
	lastProgressPrintDone = current;
}

#pragma mark __Inpput Callback Definitions

static AURenderCallbackStruct sInputCallback; // we set one of these two callbacks based on AU type

static OSStatus InputCallback (void 			*inRefCon, 
					AudioUnitRenderActionFlags 	*ioActionFlags, 
					const AudioTimeStamp 		*inTimeStamp, 
					UInt32 						inBusNumber, 
					UInt32 						inNumberFrames, 
					AudioBufferList 			*ioData)
{
	CAAudioFile &readFile = *(static_cast<CAAudioFile*>(inRefCon));

													#if CAAUDIOFILE_PROFILE 
														UInt64 now = CAHostTimeBase::GetCurrentTime(); 
													#endif

	readFile.SeekToPacket (SInt64(inTimeStamp->mSampleTime));
	readFile.ReadPackets (inNumberFrames, ioData);

													#if CAAUDIOFILE_PROFILE 
														sReadTime += (CAHostTimeBase::GetCurrentTime() - now); 
													#endif

	return noErr;
}

static OSStatus FConvInputCallback (void 			*inRefCon, 
					AudioUnitRenderActionFlags 	*ioActionFlags, 
					const AudioTimeStamp 		*inTimeStamp, 
					UInt32 						inBusNumber, 
					UInt32 						inNumberFrames, 
					AudioBufferList 			*ioData)
{
	CAAudioFile &readFile = *(static_cast<CAAudioFile*>(inRefCon));

												#if CAAUDIOFILE_PROFILE 
													UInt64 now = CAHostTimeBase::GetCurrentTime(); 
												#endif

		// this test is ONLY needed in case of processing with a Format Converter type of AU
		// in all other cases, the CAAUProcessor class will NEVER call you for input
		// beyond the end of the file....
	if (SInt64(inTimeStamp->mSampleTime) >= readFile.GetNumberPackets()) {
		return -1;
	}
	
	readFile.SeekToPacket (SInt64(inTimeStamp->mSampleTime));
	UInt32 readPackets = inNumberFrames;
		
		// also, have to do this for a format converter AU - otherwise we'd just read what we're told
	if (SInt64(inTimeStamp->mSampleTime + inNumberFrames) > readFile.GetNumberPackets()) {
		// first set this to zero as we're only going to read a partial number of frames
		AudioBuffer *buf = ioData->mBuffers;
		for (UInt32 i = ioData->mNumberBuffers; i--; ++buf)
			memset((Byte *)buf->mData, 0, buf->mDataByteSize);
		readPackets = UInt32 (readFile.GetNumberPackets() - SInt64(inTimeStamp->mSampleTime));
	}
	
	readFile.ReadPackets (readPackets, ioData);

													#if CAAUDIOFILE_PROFILE 
														sReadTime += (CAHostTimeBase::GetCurrentTime() - now); 
													#endif

	return noErr;
}

#pragma mark __Utility Helpers

CFPropertyListRef	 ReadPresetFromPresetFile (char* filePath)
{	
	if (!filePath)
		return NULL;
	
	FSRef ref;
	if (FSPathMakeRef((UInt8 *)filePath, &ref, NULL))
		return NULL;
		
	CFURLRef fileURL = CFURLCreateFromFSRef (kCFAllocatorDefault, &ref);

	CFDataRef         resourceData = NULL;
	SInt32            result;
    
   // Read the XML file.
   Boolean status = CFURLCreateDataAndPropertiesFromResource (kCFAllocatorDefault, (CFURLRef)fileURL,
                                                                &resourceData,	// place to put file data
                                                                NULL, NULL, &result);
        if (status == false || result) {
            if (resourceData) 
				CFRelease (resourceData);
            return NULL;
        }
    
	CFStringRef errString = NULL;
	CFPropertyListRef theData = CFPropertyListCreateFromXMLData (kCFAllocatorDefault, resourceData,  
													kCFPropertyListImmutable, &errString);
        if (theData == NULL || errString) {
            if (resourceData) 
				CFRelease (resourceData);
			if (errString)
				CFRelease (errString);
            return NULL;
       }
	
	CFRelease (resourceData);
    
	return theData;
}

#pragma mark __the setup code

#define OFFLINE_AU_CMD 		"[-au TYPE SUBTYPE MANU] The Audio Unit component description\n\t"
#define INPUT_FILE	 		"[-i /Path/To/File] The file that is to be processed.\n\t"
#define OUTPUT_FILE			"[-o /Path/To/File/To/Create] This will be in the same format as the input file\n\t"
#define AU_PRESET_CMD		"[-p /Path/To/AUPreset/File] Specify an AU Preset File to establish the state of the AU\n\t"

static char* usageStr = "Usage: AU Process\n\t" 
				OFFLINE_AU_CMD 
				INPUT_FILE
				OUTPUT_FILE
				AU_PRESET_CMD;

static int		StrToOSType(const char *str, OSType &t)
{
	char buf[4];
	const char *p = str;
	int x;
	for (int i = 0; i < 4; ++i) {
		if (*p != '\\') {
			if ((buf[i] = *p++) == '\0')
				goto fail;
		} else {
			if (*++p != 'x') goto fail;
			if (sscanf(++p, "%02X", &x) != 1) goto fail;
			buf[i] = x;
			p += 2;
		}
	}
	t = EndianU32_BtoN(*(UInt32 *)buf);
	return p - str;
fail:
	return 0;
}

int main(int argc, const char * argv[])
{
	setbuf (stdout, NULL);

// These are the variables that are set up from the input parsing
	char* srcFilePath = NULL;
	char* destFilePath = NULL;
	char* auPresetFile = NULL;
	
	OSType manu, subType, type = 0;
	
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp (argv[i], "-au") == 0) {
            if ( (i + 3) < argc ) {                
                StrToOSType (argv[i + 1], type);
                StrToOSType (argv[i + 2], subType);
                StrToOSType (argv[i + 3], manu);
				i += 3;
			} else {
				printf ("Which Audio Unit:\n%s", usageStr);
				exit(1);
			}
		}
		else if (strcmp (argv[i], "-i") == 0) {
			srcFilePath = const_cast<char*>(argv[++i]);
		}
		else if (strcmp (argv[i], "-o") == 0) {
			destFilePath = const_cast<char*>(argv[++i]);
		}
		else if (strcmp (argv[i], "-p") == 0) {
			auPresetFile = const_cast<char*>(argv[++i]);
		}
		else {
			printf ("%s\n", usageStr);
			exit(1);
		}
	}
	
	if (!type || !srcFilePath || !destFilePath) {
		printf ("%s\n", usageStr);
		exit(1);
	}

			// delete pre-existing output file
	FSRef destFSRef;
	if (FSPathMakeRef((UInt8 *)destFilePath, &destFSRef, NULL) == noErr) {
		// output file exists - delete it
		if (FSDeleteObject(&destFSRef)) {
			printf ("Cannot Delete Output File\n");
			exit(1);
		}
	}
		
	
	CAComponentDescription desc(type, subType, manu);
	
	CFPropertyListRef presetDict = ReadPresetFromPresetFile(auPresetFile);

		// the num of frames to use when processing the file with the Render call
	UInt32 maxFramesToUse = 32768;
		
		// in some settings (for instance a delay with 100% feedback) tail time is essentially infinite
		// so you should safeguard the final OL render stage (post process) which is aimed at pulling the tail through
		// if you want to bypass this completely, just set this to zero.
	Float64 maxTailTimeSecs = 10.;
	
#pragma mark -
#pragma mark __ The driving code
#pragma mark -

	try 
	{
		CAComponent comp(desc);
			
			 // CAAUProcessor's constructor throws... so make sure the component is valid
		if (comp.IsValid() == false) {
			printf ("Can't Find Component\n");
			desc.Print();
			exit(1);
		}
			
		CAAUProcessor processor(comp);
													processor.AU().Print();
		
		CAAudioFile srcFile;
		CAAudioFile destFile; 
		
		srcFile.Open(srcFilePath);
		destFile.PrepareNew (srcFile.GetFileDataFormat(), srcFile.GetFileDataFormat().mSampleRate);
		
		CAStreamBasicDescription procFormat (srcFile.GetFileDataFormat());
		procFormat.SetCanonical (srcFile.GetFileDataFormat().NumberChannels(), false);

													printf ("Processing Format:\n\t");
													procFormat.Print();

		srcFile.SetClientFormat (procFormat);
		destFile.SetClientFormat (procFormat);
		
		AUOutputBL outputList(procFormat);

		destFile.Create (destFilePath, 'AIFF');

		if (desc.IsFConv()) {
			maxFramesToUse = maxFramesToUse > 512 ? 512 : maxFramesToUse; 
			// some format converter's can call you several times in small granularities
			// so you can't use a large buffer to render or you won't return all of the input data
			// this also lessens the final difference between what you should get and what you do
			// converter units *really* should have a version that are offline AU's to 
			// handle this for you.
			sInputCallback.inputProc = FConvInputCallback;
		} else
			sInputCallback.inputProc = InputCallback;
	
		sInputCallback.inputProcRefCon = &srcFile;
		
		OSStatus result;
		require_noerr (result = processor.EstablishInputCallback (sInputCallback), home);
		require_noerr (result = processor.SetMaxFramesPerRender (maxFramesToUse), home); 
		processor.SetMaxTailTime (maxTailTimeSecs);
		require_noerr (result = processor.Initialize (procFormat, srcFile.GetNumberPackets()), home);
		if (presetDict) {
			require_noerr (result = processor.SetAUPreset (presetDict), home);
			CFRelease (presetDict);
		}
			// this does ALL of the preflighting.. could be specialise for an OfflineAU type
			// to do this piecemeal and do a progress bar by using the OfflineAUPreflight method
		require_noerr (result = processor.Preflight (), home);
		
		bool isDone; isDone = false;
		bool needsPostProcessing;
		bool isSilence;
		UInt32 numFrames; numFrames = processor.MaxFramesPerRender();

		sReadTime = 0;
					
PRINT_MARKS();
			// this is the render loop
		while (!isDone) 
		{
											#if CAAUDIOFILE_PROFILE 
												UInt64 now = CAHostTimeBase::GetCurrentTime(); 
											#endif
			outputList.Prepare(); // have to do this every time...
			require_noerr (result = processor.Render (outputList.ABL(), numFrames, isSilence, &isDone,
											&needsPostProcessing), home);
											#if CAAUDIOFILE_PROFILE 
												sRenderTime += (CAHostTimeBase::GetCurrentTime() - now);
											#endif

PRINT_PROGRESS(processor.GetOLPercentComplete());

			if (numFrames)
				destFile.WritePackets (numFrames, outputList.ABL());
		}
			
			// this is the postprocessing if needed
		if (needsPostProcessing) 
		{
			isDone = false;
			numFrames = processor.MaxFramesPerRender();
			while (!isDone) {
				outputList.Prepare(); // have to do this every time...
											#if CAAUDIOFILE_PROFILE 
												UInt64 now = CAHostTimeBase::GetCurrentTime(); 
											#endif
				require_noerr (result = processor.PostProcess (outputList.ABL(), numFrames, 
													isSilence, isDone), home);
											#if CAAUDIOFILE_PROFILE 
												sRenderTime += (CAHostTimeBase::GetCurrentTime() - now); 
											#endif

PRINT_PROGRESS(processor.GetOLPercentComplete());

				if (numFrames)
					destFile.WritePackets (numFrames, outputList.ABL());
			}
		}

printf ("\n");

home:
		if (result) {
			printf ("Exit with bad result:%ld\n", result);
			exit(result);
		}
		
		
#if CAAUDIOFILE_PROFILE
	destFile.Close(); 
	// this flushes any remaing data to be written to the disk. 
	// the source file is closed in its destructor of course

	printf ("Read File Time:%.2f secs for %lld packets, wrote %lld packets\n", 
						(CAHostTimeBase::ConvertToNanos (sReadTime) / 1.0e9),
						srcFile.GetNumberPackets(), destFile.GetNumberPackets());

	if (destFile.GetNumberPackets() == srcFile.GetNumberPackets()) {
		printf ("\tWrote the same number of packets as read\n");
	} else {
		bool expectationMet = !desc.IsOffline(); // we don't have any expectations for offline AU's
		if (processor.LatencySampleCount() || processor.TailSampleCount()) {
			if (destFile.GetNumberPackets() - srcFile.GetNumberPackets() == processor.TailSampleCount())
				expectationMet = true;
			if (expectationMet)	
				printf ("Correctly wrote \'Read Size + Tail\'. ");
			printf ("AU reports (samples): %ld latency, %ld tail\n", 
									processor.LatencySampleCount(), processor.TailSampleCount());
		}
		if (expectationMet == false) 
		{
			if (destFile.GetNumberPackets() > srcFile.GetNumberPackets()) {
				printf ("\tWrote %lld packets (%.2f secs) more than read\n", 
							(destFile.GetNumberPackets() - srcFile.GetNumberPackets()), 
							((destFile.GetNumberPackets() - srcFile.GetNumberPackets()) / procFormat.mSampleRate));
			} else {
				printf ("\tRead %lld packets (%.2f secs) more than wrote\n", 
							(srcFile.GetNumberPackets() - destFile.GetNumberPackets()), 
							((srcFile.GetNumberPackets() - destFile.GetNumberPackets()) / procFormat.mSampleRate));
			}
		}
	}
	
	Float64 renderTimeSecs = CAHostTimeBase::ConvertToNanos (sRenderTime - sReadTime) / 1.0e9;
	printf ("Total Render Time:%.2f secs, using render slice size of %ld frames\n", 
							renderTimeSecs, maxFramesToUse);
	
	Float64 cpuUsage = (renderTimeSecs / (srcFile.GetNumberPackets() / procFormat.mSampleRate)) * 100.;
	printf ("CPU Usage for Render Time:%.2f%%\n", cpuUsage);

	CFStringRef str = comp.GetCompName();
	char* cstr = (char*)malloc (CFStringGetLength (str) + 1);
	CFStringGetCString (str, cstr, (CFStringGetLength (str) + 1), kCFStringEncodingASCII);
	PerfResult("AudioUnitProcess", 0, cstr, cpuUsage, "%realtime");
	free (cstr);
#endif


	}
	catch (CAXException &e) {
		char buf[256];
		printf("Error: %s (%s)\n", e.mOperation, e.FormatError(buf));
		exit(1);
	}
	catch (...) {
		printf("An unknown error occurred\n");
		exit(1);
	}
	
	return 0;
}

