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
	afrecord.cpp
	
=============================================================================*/

#include "CAAudioFileRecorder.h"
#include "CAXException.h"
#include "CAAudioFileFormats.h"
#include <unistd.h>

static void usage()
{
	CAAudioFileFormats *theFileFormats = CAAudioFileFormats::Instance();

	fprintf(stderr,
			"Usage:\n"
			"%s [option...] audio_file\n\n"
			"Options: (may appear before or after arguments)\n"
			"    { -f | --file } file_format:\n"
			, getprogname());

	for (int i = 0; i < theFileFormats->mNumFileFormats; ++i) {
		CAAudioFileFormats::FileFormatInfo *ffi = &theFileFormats->mFileFormats[i];
		char buf[20];
		char fmtName[256] = { 0 };
		if (ffi->mFileTypeName)
			CFStringGetCString(ffi->mFileTypeName, fmtName, sizeof(fmtName), kCFStringEncodingASCII);
		fprintf(stderr, "        '%s' = %s\n", OSTypeToStr(buf, ffi->mFileTypeID), fmtName);
		fprintf(stderr, "                   data_formats: ");
		
		int count = 0;
		for (int j = 0; j < ffi->mNumDataFormats; ++j) {
			CAAudioFileFormats::DataFormatInfo *dfi = &ffi->mDataFormats[j];
			if (dfi->mFormatID == kAudioFormatLinearPCM) {
				for (int k = 0; k < dfi->mNumVariants; ++k) {
					if (++count == 6) {
						fprintf(stderr, "\n                                 ");
						count = 0;
					}
					AudioStreamBasicDescription *asbd = &dfi->mVariants[k];
					if (asbd->mFormatFlags & ~(kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsFloat))
						fprintf(stderr, "(%08lx/%ld) ", asbd->mFormatFlags, asbd->mBitsPerChannel);
					else {
						fprintf(stderr, "%s",
							(asbd->mFormatFlags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE");
						if (asbd->mFormatFlags & kAudioFormatFlagIsFloat)
							fprintf(stderr, "F");
						else
							fprintf(stderr, "%sI",
								(asbd->mFormatFlags & kAudioFormatFlagIsSignedInteger) ? "" : "U");
						fprintf(stderr, "%ld ", asbd->mBitsPerChannel);
					}
				}
			} else {
				if (++count == 6) {
					fprintf(stderr, "\n                                 ");
					count = 0;
				}
				fprintf(stderr, "'%s' ", OSTypeToStr(buf, dfi->mFormatID));
			}
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr,
			"    { -d | --data } data_format[@sample_rate_hz]:\n"
			"        [-][BE|LE]{F|[U]I}{8|16|24|32|64}          (PCM)\n"
			"            e.g. -BEI16 -F32@44100\n"
			"        or a data format appropriate to file format, as above\n"
			"    { -c | --channels } number_of_channels\n"
			"        add/remove channels without regard to order\n"
			"    { -l | --channellayout } layout_tag\n"
			"        layout_tag: name of a constant from CoreAudioTypes.h\n"
			"          (prefix \"kAudioChannelLayoutTag_\" may be omitted)\n"
			"        if specified once, applies to output file; if twice, the first\n"
			"          applies to the input file, the second to the output file\n"
			"    { -b | --bitrate } bit_rate_bps\n"
			"         e.g. 128000\n"
			"    { -q | --quality } quality\n"
			"        quality: 0-127\n"
			"    { -v | --verbose }\n"
			"        print progress verbosely\n"
			"    { -p | --profile }\n"
			"        collect and print performance profile\n"
			);
	exit(1);
}

static void	MissingArgument()
{
	fprintf(stderr, "missing argument\n\n");
	usage();
}

static OSType Parse4CharCode(const char *arg, const char *name)
{
	OSType t;
	StrToOSType(arg, t);
	if (t == 0) {
		fprintf(stderr, "invalid 4-char-code argument for %s: '%s'\n\n", name, arg);
		usage();
	}
	return t;
}

static int	ParseInt(const char *arg, const char *name)
{
	int x;
	if (sscanf(arg, "%d", &x) != 1) {
		fprintf(stderr, "invalid integer argument for %s: '%s'\n\n", name, arg);
		usage();
	}
	return x;
}

/*
struct AudioStreamBasicDescription
{
	Float64	mSampleRate;		//	the native sample rate of the audio stream
	UInt32	mFormatID;			//	the specific encoding type of audio stream
	UInt32	mFormatFlags;		//	flags specific to each format
	UInt32	mBytesPerPacket;	//	the number of bytes in a packet
	UInt32	mFramesPerPacket;	//	the number of frames in each packet
	UInt32	mBytesPerFrame;		//	the number of bytes in a frame
	UInt32	mChannelsPerFrame;	//	the number of channels in each frame
	UInt32	mBitsPerChannel;	//	the number of bits in each channel
	UInt32	mReserved;			//	reserved, pads the structure out to force 8 byte alignment
};
*/
static bool ParseStreamDescription(const char *inTextDesc, CAStreamBasicDescription &fmt)
{
	const char *p = inTextDesc;
	int bitdepth = 0;
	CAAudioFileFormats *theFileFormats = CAAudioFileFormats::Instance();
	
	memset(&fmt, 0, sizeof(fmt));
	OSType formatID;
	int x = StrToOSType(p, formatID);
	if (theFileFormats->IsKnownDataFormat(formatID)) {
		p += x;
		fmt.mFormatID = formatID;
	}
	
	if (fmt.mFormatID == 0) {
		// unknown format, assume LPCM
		if (p[0] == '-')	// previously we required a leading dash on PCM formats
			// pcm
			++p;
		fmt.mFormatID = kAudioFormatLinearPCM;
		fmt.mFormatFlags = kAudioFormatFlagIsPacked;
		fmt.mFramesPerPacket = 1;
		fmt.mChannelsPerFrame = 1;
		bool isUnsigned = false;
	
		if (p[0] == 'B' && p[1] == 'E') {
			fmt.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
			p += 2;
		} else if (p[0] == 'L' && p[1] == 'E') {
			p += 2;
		} else {
			// default is native-endian
#if TARGET_RT_BIG_ENDIAN
			fmt.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
#endif
		}
		if (p[0] == 'F') {
			fmt.mFormatFlags |= kAudioFormatFlagIsFloat;
			++p;
		} else {
			if (p[0] == 'U') {
				isUnsigned = true;
				++p;
			}
			if (p[0] == 'I')
				++p;
			else {
				fprintf(stderr, "The format '%s' is unknown or an unparseable PCM format specifier\n", inTextDesc);
				goto Bail;
			}
		}
		
		while (isdigit(*p))
			bitdepth = 10 * bitdepth + *p++ - '0';
		if (fmt.mFormatFlags & kAudioFormatFlagIsFloat) {
			if (bitdepth != 32 && bitdepth != 64) {
				fprintf(stderr, "Valid float bitdepths are 32 and 64\n");
				goto Bail;
			}
		} else {
			if (bitdepth != 8 && bitdepth != 16 && bitdepth != 24 && bitdepth != 32) {
				fprintf(stderr, "Valid integer bitdepths are 8, 16, 24, and 32\n");
				goto Bail;
			}
			if (!isUnsigned)
				fmt.mFormatFlags |= kAudioFormatFlagIsSignedInteger;
		}
		fmt.mBitsPerChannel = bitdepth;
		fmt.mBytesPerPacket = fmt.mBytesPerFrame = bitdepth / 8;
	}
	if (*p == '@') {
		++p;
		while (isdigit(*p))
			fmt.mSampleRate = 10 * fmt.mSampleRate + (*p++ - '0');
	}
	if (*p != '\0')
		goto Bail;
	return true;

Bail:
	fprintf(stderr, "Invalid format string: %s\n", inTextDesc);
	fprintf(stderr, "Syntax of format strings is: [-][BE|LE]{F|I|UI}{8|16|24|32|64}[@sample_rate_hz]\n");
	return false;
}

static void Record(CAAudioFileRecorder &recorder)
{
	recorder.Start();
	printf("Recording, press any key to stop:");
	fflush(stdout);
	getchar();
	//sleep(10);
	recorder.Stop();
}

int main(int argc, const char *argv[])
{
	const char *recordFileName = NULL;

	// set up defaults
	AudioFileTypeID filetype = kAudioFileAIFFType;

	CAStreamBasicDescription dataFormat;
	dataFormat.mSampleRate = 44100.;	// later get this from the hardware
	dataFormat.mFormatID = kAudioFormatLinearPCM;
	dataFormat.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	dataFormat.mFramesPerPacket = 1;
	dataFormat.mChannelsPerFrame = 2;
	dataFormat.mBitsPerChannel = 16;
	dataFormat.mBytesPerPacket = dataFormat.mBytesPerFrame = 4;
	
	SInt32 bitrate = -1, quality = -1;
	
	// parse arguments
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (arg[0] != '-') {
			if (recordFileName != NULL) {
				fprintf(stderr, "may only specify one record file\n");
				usage();
			}
			recordFileName = arg;
		} else {
			arg += 1;
			if (arg[0] == 'f' || !strcmp(arg, "-file")) {
				if (++i == argc) MissingArgument();
				filetype = Parse4CharCode(argv[i], "-f | --file");
			} else if (arg[0] == 'd' || !strcmp(arg, "-data")) {
				if (++i == argc) MissingArgument();
				if (!ParseStreamDescription(argv[i], dataFormat))
					usage();
			} else if (arg[0] == 'b' || !strcmp(arg, "-bitrate")) {
				if (++i == argc) MissingArgument();
				bitrate = ParseInt(argv[i], "-b | --bitrate");
			} else if (arg[0] == 'q' || !strcmp(arg, "-quality")) {
				if (++i == argc) MissingArgument();
				quality = ParseInt(argv[i], "-q | --quality");
			} else {
				fprintf(stderr, "unknown argument: %s\n\n", arg - 1);
				usage();
			}
		}
	}
	
	if (recordFileName == NULL)
		usage();
	
	unlink(recordFileName);
	
	if (dataFormat.IsPCM())
		dataFormat.ChangeNumberChannels(2, true);
	else
		dataFormat.mChannelsPerFrame = 2;
	
	try {
		const int kNumberBuffers = 3;
		const unsigned kBufferSize = 0x8000;
		CAAudioFileRecorder recorder(kNumberBuffers, kBufferSize);
		recorder.SetFile(recordFileName, filetype, dataFormat, NULL);
		
		CAAudioFile &recfile = recorder.GetFile();
#warning bitrate/quality not implemented
#if 1
		if (bitrate >= 0)
			recfile.SetConverterProperty(kAudioConverterEncodeBitRate, 
				sizeof(UInt32), &bitrate);
		if (quality >= 0)
			recfile.SetConverterProperty(kAudioConverterCodecQuality, 
					sizeof(UInt32), &quality);
#endif

		Record(recorder);
	}
	catch (CAXException &e) {
		char buf[256];
		fprintf(stderr, "Error: %s (%s)\n", e.mOperation, CAXException::FormatError(buf, e.mError));
		return 1;
	}
	catch (...) {
		fprintf(stderr, "An unknown error occurred\n");
		return 1;
	}
	return 0;
}

