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
	CAAudioFileConverter.cpp
	
=============================================================================*/

#include "CAAudioFileConverter.h"
#include "CAChannelLayouts.h"
#include <libgen.h>
#include <sys/stat.h>
#include <algorithm>
#include "CAHostTimeBase.h"
#include "CAXException.h"

CAAudioFileConverter::ConversionParameters::ConversionParameters() :
	flags(0)
{
	memset(&input, 0, sizeof(input));
	memset(&output, 0, sizeof(output));
	output.channels = -1;
	output.bitRate = -1;
	output.quality = -1;
}

CAAudioFileConverter::CAAudioFileConverter() :
	mReadBuffer(NULL),
	mReadPtrs(NULL)
{
	mOutName[0] = '\0';
}

CAAudioFileConverter::~CAAudioFileConverter()
{
}

void	CAAudioFileConverter::GenerateOutputFileName(const char *inputFilePath, 
						const CAStreamBasicDescription &inputFormat,
						const CAStreamBasicDescription &outputFormat, OSType outputFileType, 
						char *outName)
{
	struct stat sb;
	char inputDir[256];
	char inputBasename[256];
	
	strcpy(inputDir, dirname(inputFilePath));
	const char *infname = basename(inputFilePath);
	const char *inext = strrchr(infname, '.');
	if (inext == NULL) strcpy(inputBasename, infname);
	else {
		int n;
		memcpy(inputBasename, infname, n = inext - infname);
		inputBasename[n] = '\0';
	}
	
	CFArrayRef exts;
	UInt32 propSize = sizeof(exts);
	XThrowIfError(AudioFileGetGlobalInfo(kAudioFileGlobalInfo_ExtensionsForType,
		sizeof(OSType), &outputFileType, &propSize, &exts), "generate output file name");
	char outputExt[32];
	CFStringRef cfext = (CFStringRef)CFArrayGetValueAtIndex(exts, 0);
	CFStringGetCString(cfext, outputExt, sizeof(outputExt), kCFStringEncodingUTF8);
	CFRelease(exts);
	
	// 1. olddir + oldname + newext
	sprintf(outName, "%s/%s.%s", inputDir, inputBasename, outputExt);
	if (lstat(outName, &sb)) return;

	if (outputFormat.IsPCM()) {
		// If sample rate changed:
		//	2. olddir + oldname + "-SR" + newext
		if (inputFormat.mSampleRate != outputFormat.mSampleRate && outputFormat.mSampleRate != 0.) {
			sprintf(outName, "%s/%s-%.0fk.%s", inputDir, inputBasename, outputFormat.mSampleRate/1000., outputExt);
			if (lstat(outName, &sb)) return;
		}
		// If bit depth changed:
		//	3. olddir + oldname + "-bit" + newext
		if (inputFormat.mBitsPerChannel != outputFormat.mBitsPerChannel) {
			sprintf(outName, "%s/%s-%ldbit.%s", inputDir, inputBasename, outputFormat.mBitsPerChannel, outputExt);
			if (lstat(outName, &sb)) return;
		}
	}
	
	// maybe more with channels/layouts? $$$
	
	// now just append digits
	for (int i = 1; ; ++i) {
		sprintf(outName, "%s/%s-%d.%s", inputDir, inputBasename, i, outputExt);
		if (lstat(outName, &sb)) return;
	}
}

void	CAAudioFileConverter::PrintFormats(const CAAudioChannelLayout *origSrcFileLayout)
{
	const CAAudioChannelLayout &srcFileLayout = mSrcFile.GetFileChannelLayout();
	const CAAudioChannelLayout &destFileLayout = mDestFile.GetFileChannelLayout();
	
	// see where we've gotten
	if (mParams->flags & kOpt_Verbose) {
		printf("Formats:\n");
		mSrcFile.GetFileDataFormat().PrintFormat(stdout, "  ", "Input file   ");
		if (srcFileLayout.IsValid()) {
			printf("                 %s", 
				CAChannelLayouts::ConstantToString(srcFileLayout.Tag()));
			if (srcFileLayout.IsValid() && origSrcFileLayout != NULL &&
			srcFileLayout != *origSrcFileLayout)
				printf(" -- overriding layout %s in file", 
					CAChannelLayouts::ConstantToString(origSrcFileLayout->Tag()));
			printf("\n");
		}
		mDestFile.GetFileDataFormat().PrintFormat(stdout, "  ", "Output file  ");
		if (destFileLayout.IsValid())
			printf("                 %s\n", 
				CAChannelLayouts::ConstantToString(destFileLayout.Tag()));
		if (mSrcFile.HasConverter()) {
			mSrcFile.GetClientDataFormat().PrintFormat(stdout, "  ", "Input client ");
			CAShow(mSrcFile.GetConverter());
		}
		if (mDestFile.HasConverter()) {
			mDestFile.GetClientDataFormat().PrintFormat(stdout, "  ", "Output client");
			CAShow(mDestFile.GetConverter());
		}
	}			
}

void	CAAudioFileConverter::ConvertFile(const ConversionParameters &params)
{
	FSRef destFSRef;
	UInt32 propertySize;
	CAStreamBasicDescription destFormat(params.output.dataFormat);
	CAStreamBasicDescription srcClientFormat, destClientFormat;
	CAAudioChannelLayout origSrcFileLayout, srcFileLayout, destFileLayout;
	bool createdOutputFile = false;
	
	mParams = &params;
	mReadBuffer = NULL;
	mReadPtrs = NULL;
	CABufferList *writeBuffer = NULL;
	CABufferList *writePtrs = NULL;
	
#if CAAUDIOFILE_PROFILE
	if (params.flags & kOpt_Profile) {
		mSrcFile.EnableProfiling(true);
		mDestFile.EnableProfiling(true);
	}
#endif

	try {
		// find and open the input file
		if (params.input.audioFileID)
			mSrcFile.Wrap(params.input.audioFileID, false);
		else
			mSrcFile.Open(params.input.filePath);
		// get input file's format
		const CAStreamBasicDescription &srcFormat = mSrcFile.GetFileDataFormat();
		if (mParams->flags & kOpt_Verbose) {
			printf("Input file: %s, %qd packets\n", params.input.filePath ? basename(params.input.filePath) : "?", 
				mSrcFile.GetNumberPackets());
		}
		bool encoding = !destFormat.IsPCM();
		bool decoding = !srcFormat.IsPCM();
		
		// prepare output file's format

		// source channel layout
		srcFileLayout = mSrcFile.GetFileChannelLayout();
		origSrcFileLayout = srcFileLayout;
		if (params.input.channelLayoutTag != 0) {
			XThrowIf(AudioChannelLayoutTag_GetNumberOfChannels(params.input.channelLayoutTag)
				!= srcFormat.mChannelsPerFrame, -1, "input channel layout has wrong number of channels for file");
			srcFileLayout = CAAudioChannelLayout(params.input.channelLayoutTag);
			mSrcFile.SetFileChannelLayout(srcFileLayout);
		}
		
		// destination channel layout
		int outChannels = params.output.channels;
		if (params.output.channelLayoutTag != 0) {
			// use the one specified by caller, if any
			destFileLayout = CAAudioChannelLayout(params.output.channelLayoutTag);
		} else if (srcFileLayout.IsValid()) {
			// otherwise, assume the same as the source, if any
			destFileLayout = srcFileLayout;
		}
		if (destFileLayout.IsValid()) {
			// the output channel layout specifies the number of output channels
			if (outChannels != -1)
				XThrowIf((unsigned)outChannels != destFileLayout.NumberChannels(), -1,
					"output channel layout has wrong number of channels");
			else
				outChannels = destFileLayout.NumberChannels();
		}

		// adjust the output format's channels; output.channels overrides the channels
		if (outChannels == -1)
			outChannels = srcFormat.mChannelsPerFrame;
		if (outChannels > 0) {
			destFormat.mChannelsPerFrame = outChannels;
			destFormat.mBytesPerPacket *= outChannels;
			destFormat.mBytesPerFrame *= outChannels;
		}
		
		// use AudioFormat API to clean up the output format
		propertySize = sizeof(AudioStreamBasicDescription);
		XThrowIfError(AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, 
				0, NULL, &propertySize, &destFormat),
				"get destination format info");
		
		// initialize the output file object (doesn't create file yet)
		mDestFile.PrepareNew(destFormat, destFileLayout.IsValid() ? &destFileLayout : NULL);
		
		{
			CAAudioChannelLayout srcClientLayout, destClientLayout;
			
			// not mixing
			srcClientFormat = srcFormat;
			destClientFormat = destFormat;
			
			if (encoding) {
				if (decoding) {
					// transcoding
//					XThrowIf(encoding && decoding, -1, "transcoding not currently supported");
					
					if (srcFormat.mChannelsPerFrame > 2 || destFormat.mChannelsPerFrame > 2)
						CAXException::Warning("Transcoding multichannel audio may not handle channel layouts correctly", 0);
					srcClientFormat.SetCanonical(std::min(srcFormat.mChannelsPerFrame, destFormat.mChannelsPerFrame), true);
					srcClientFormat.mSampleRate = std::max(srcFormat.mSampleRate, destFormat.mSampleRate);
					mSrcFile.SetClientFormat(srcClientFormat, NULL);
					
					destClientFormat = srcClientFormat;
				} else {
					// encoding
					destClientFormat = srcFormat;
				}
				destClientLayout = srcFileLayout.IsValid() ? srcFileLayout : destFileLayout;

				mDestFile.SetClientFormat(destClientFormat, &destClientLayout);
			} else {
				// decoding or PCM->PCM
				srcClientFormat = destFormat;
				srcClientLayout = destFileLayout;
				mSrcFile.SetClientFormat(srcClientFormat, &srcClientLayout);
			}
		}
		XThrowIf(srcClientFormat.mBytesPerPacket == 0, -1, "source client format not PCM"); 
		XThrowIf(destClientFormat.mBytesPerPacket == 0, -1, "dest client format not PCM"); 		
		if (encoding) {
			// set the bitrate
			if (params.output.bitRate != -1) {
				if (mParams->flags & kOpt_Verbose)
					printf("bitrate = %ld\n", params.output.bitRate);
				mDestFile.SetConverterProperty(kAudioConverterEncodeBitRate, 
					sizeof(UInt32), &params.output.bitRate);
			}

			// set the quality
			if (params.output.quality != -1) {
				if (mParams->flags & kOpt_Verbose)
					printf("quality = %ld\n", params.output.quality);
				mDestFile.SetConverterProperty(kAudioConverterCodecQuality, 
					sizeof(UInt32), &params.output.quality);
			}
		}
		
		if (params.output.filePath == NULL) {
			GenerateOutputFileName(params.input.filePath, srcFormat,
						destFormat, params.output.fileType, mOutName);
		} else
			strcpy(mOutName, params.output.filePath);
		
		// deal with pre-existing output file
		if (FSPathMakeRef((UInt8 *)mOutName, &destFSRef, NULL) == noErr) {
			XThrowIf(!(params.flags & kOpt_OverwriteOutputFile), 1, "overwrite output file");
				// not allowed to overwrite
			// output file exists - delete it
			XThrowIfError(FSDeleteObject(&destFSRef), "delete output file");
		}
		// create the output file
		mDestFile.Create(mOutName, params.output.fileType);
		createdOutputFile = true;
		
		PrintFormats(&origSrcFileLayout);

		// prepare I/O buffers
		const UInt32 bytesToRead = 0x8000;
		const UInt32 packetsToRead = bytesToRead;	// OK, ReadPackets will limit as appropriate
		
		mReadBuffer = CABufferList::New("readbuf", srcClientFormat);
		mReadBuffer->AllocateBuffers(bytesToRead);
		mReadPtrs = CABufferList::New("readptrs", srcClientFormat);
		
		while (true) {
			XThrowIf(Progress(mSrcFile.TellPacket(), mSrcFile.GetNumberPackets()), userCanceledErr, "user stopped");
			UInt32 nPackets = packetsToRead;
			mReadPtrs->SetFrom(mReadBuffer);
			AudioBufferList *readbuf = &mReadPtrs->GetModifiableBufferList();
			
			mSrcFile.ReadPackets(nPackets, readbuf);
			if (nPackets == 0)
				break;

			mDestFile.WritePackets(nPackets, readbuf);
		}
	}
	catch (...) {
		delete mReadBuffer;
		delete mReadPtrs;
		delete writeBuffer;
		delete writePtrs;
		if (!createdOutputFile)
			PrintFormats(&origSrcFileLayout);
		mSrcFile.Close();
		if (createdOutputFile)
			mDestFile.Delete();
		throw;
	}
	delete mReadBuffer;
	delete mReadPtrs;
	delete writeBuffer;
	delete writePtrs;
	mSrcFile.Close();
	mDestFile.Close();
	if (mParams->flags & kOpt_Verbose)
		printf("Output file: %s, %qd packets\n", basename(mOutName), mDestFile.GetNumberPackets());
}

