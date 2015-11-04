/*	Copyright © 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	afconvert.cpp
	
	
	Revision 1.25  2007/03/29 23:27:24  skuo
	added comments to the man page for encoding modes
	
	Revision 1.24  2007/02/14 01:29:17  bills
	add a -h (help) command
	
	Revision 1.23  2006/10/25 17:18:43  dwyatt
	-c is --channels, not --src-complexity
	
	Revision 1.22  2006/06/07 01:51:18  asynth
	4408414 sample rate converter complexity property
	
	Revision 1.21  2006/05/19 18:59:39  asynth
	'mast' -> 'bats'
	
	Revision 1.20  2006/05/12 00:30:37  ealdrich
	Move call to ParseOtherOption()
	
	Revision 1.19  2006/05/11 22:11:46  ealdrich
	Add the ability to set arbitrary user defined properties.
	
	Revision 1.18  2006/05/11 02:40:12  asynth
	4408414 mastering quality SRC algorithm
	
	Revision 1.17  2005/09/21 02:55:58  dwyatt
	don't use -p for prime-method; add --tag
	
	Revision 1.16  2005/09/15 19:49:46  dwyatt
	add option for changing converter prime method
	
	Revision 1.15  2005/09/13 18:36:53  dwyatt
	add -r / --src-quality flag for sample rate converter quality
	
	Revision 1.14  2005/03/12 19:05:33  dwyatt
	add InferFileFormatFromFilename
	
	Revision 1.13  2005/03/10 02:14:01  ealdrich
	Make it build on Windows
	
	Revision 1.12  2004/12/10 02:05:26  ealdrich
	Add ability to set frames per packet.
	
	Revision 1.11  2004/10/28 23:14:51  dwyatt
	virtualize for afconvperf
	
	Revision 1.10  2004/10/15 17:09:31  dwyatt
	track CAAudioFile rewrite
	
	Revision 1.9  2004/10/05 19:21:18  dwyatt
	[3825377] support specifying format flags
	
	Revision 1.8  2004/09/14 18:37:27  ealdrich
	Allow setting the bit rate strategy (CBR, ABR, VBR) for encoded files
	
	Revision 1.7  2004/07/08 00:43:53  dwyatt
	allow setting bitdepth on non-PCM formats, e.g. alac-24
	
	Revision 1.6  2004/05/26 00:41:13  dwyatt
	remove cruft
	
	Revision 1.5  2004/05/15 01:20:12  dwyatt
	track changes to CAAudioFile etc.
	
	Revision 1.4  2004/03/05 20:40:33  dwyatt
	fix statistics
	
	Revision 1.3  2004/02/24 22:10:33  dwyatt
	document the --bitrate argument better
	
	Revision 1.2  2004/01/31 01:49:05  dwyatt
	track CAAudioFile changes
	
	Revision 1.1  2004/01/14 00:09:16  dwyatt
	moved from Source/Tests/AudioFileConverter
	
	Revision 1.11  2003/10/09 23:18:44  dwyatt
	remove dependence on PerfResult.h
	
	Revision 1.10  2003/08/27 23:43:59  dwyatt
	force use of CAAudioFileFormats::Instance()
	
	Revision 1.9  2003/08/23 04:56:37  dwyatt
	support for reporting warnings
	
	Revision 1.8  2003/08/04 23:47:49  dwyatt
	renaming, channel layouts, various
	
	Revision 1.7  2003/07/25 23:16:44  dwyatt
	use constant strings for operations in CAXException
	
	Revision 1.6  2003/07/15 17:36:30  dwyatt
	add profiling support
	
	Revision 1.5  2003/07/09 19:19:11  dwyatt
	- support automatic naming of output file
	- print available PCM formats in the same way they're specified
	- allow specifying unsigned integer PCM
	- support channel layouts
	
	Revision 1.4  2003/07/02 23:24:27  dwyatt
	code reuse
	
	Revision 1.3  2003/06/12 04:50:09  dwyatt
	ongoing
	
	Revision 1.2  2003/06/05 20:01:24  dwyatt
	refactored
	
	Revision 1.1  2003/06/02 23:11:53  dwyatt
	initial checkin
	
	Copyright (c) 2003 Apple Computer, Inc.  All Rights Reserved

	$NoKeywords: $
=============================================================================*/

/*
	afconvert
	
	command-line tool to convert an audio file to another format
*/

#include "afconvert.h"
#include <stdio.h>
#include <vector>
#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
	#include <AudioToolbox/AudioToolbox.h>
#else
	#include <CoreServices.h>
	#include <AudioToolbox.h>
#endif
#include "CAChannelLayouts.h"

#include "CAAudioFileConverter.h"
#include "CAXException.h"
#include "CAAudioFileFormats.h"
#include "AFToolsCommon.h"
#if !TARGET_OS_MAC
	#include <QTML.h>
	#include "CAWindows.h"
#endif


void AFConvertTool::usage()
{
	fprintf(stderr,
			"Usage:\n"
			"%s [option...] input_file [output_file]\n\n"
			"Options: (may appear before or after arguments)\n"
			"    { -f | --file } file_format:\n",
#if !TARGET_OS_WIN32
			getprogname());
#else
			"afconvert");
#endif
	PrintAudioFileTypesAndFormats(stderr);
	fprintf(stderr,
			"    { -d | --data } data_format[@sample_rate_hz][/format_flags][#frames_per_packet] :\n"
			"        [-][BE|LE]{F|[U]I}{8|16|24|32|64}          (PCM)\n"
			"            e.g.   BEI16   F32@44100\n"
			"        or a data format appropriate to file format, as above\n"
			"        format_flags: hex digits, e.g. '80'\n"
			"        bitdepth on non-PCM formats can be specified, e.g.: alac-24\n"
			"        Frames per packet can be specified for some encoders, e.g.: samr#12\n"
			"    { -c | --channels } number_of_channels\n"
			"        add/remove channels without regard to order\n"
			"    { -l | --channellayout } layout_tag\n"
			"        layout_tag: name of a constant from CoreAudioTypes.h\n"
			"          (prefix \"kAudioChannelLayoutTag_\" may be omitted)\n"
			"        if specified once, applies to output file; if twice, the first\n"
			"          applies to the input file, the second to the output file\n"
			"    { -b | --bitrate } bit_rate_bps\n"
			"         e.g. 128000\n"
			"    { -q | --quality } codec_quality\n"
			"        codec_quality: 0-127\n"
			"    { -r | --src-quality } src_quality\n"
			"        src_quality (sample rate converter quality): 0-127\n"
			"    { --src-complexity } src_complexity\n"
			"        src_complexity (sample rate converter complexity): line, norm, bats\n"
			"    { -v | --verbose }\n"
			"        print progress verbosely\n"
			"    { -s | --strategy } strategy\n"
			"        bitrate allocation strategy for encoding an audio track\n"
			"        0 for CBR, 1 for ABR, 2 for VBR_constrained, 3 for VBR\n"
			"    { -t | --tag }\n"
			"        If encoding to CAF, store the source file's format and name in a user chunk.\n"
			"        If decoding from CAF, use the destination format and filename found in a user chunk.\n"
			"    --prime-method method\n"
			"        decode priming method (see AudioConverter.h)\n"
			"    --no-filler\n"
			"        don't page-align audio data in the output file\n"
			"    { -u | --userproperty } property value\n"
			"        set an arbitrary property to a given value\n"
			"        property must be a four char code\n"
			"        value is a signed 32-bit integer\n"
			"        A maximum of %i properties may be set\n"
			"        e.g. use '-u vbrq <sound_quality>' to set the sound quality level (<sound_quality>: 0-127) \n"
			"             for VBR encoding strategy (i.e., -s 3) \n"
			"    { -h | --help }\n"
			"        print help\n"
			, kMaxUserProps
			);
	ExtraUsageOptions();
	exit(1);
}

void	Warning(const char *s, OSStatus error)
{
	char buf[256];
	fprintf(stderr, "*** warning: %s (%s)\n", s, CAXException::FormatError(buf, error));
}

OSType AFConvertTool::Parse4CharCode(const char *arg, const char *name)
{
	OSType t;
	StrToOSType(arg, t);
	if (t == 0) {
		fprintf(stderr, "invalid 4-char-code argument for %s: '%s'\n\n", name, arg);
		usage();
	}
	return t;
}


int		AFConvertTool::main(int argc, const char * argv[])
{
	Init();
	
	CAAudioFileConverter::ConversionParameters &params = *mParams;
	bool gotOutDataFormat = false;
	UInt32 userPropIndex = 0;
	
	CAXException::SetWarningHandler(Warning);
	
	if (argc < 2)
		usage();
	
	params.flags = CAAudioFileConverter::kOpt_OverwriteOutputFile;
	
	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (arg[0] != '-') {
			if (params.input.filePath == NULL) params.input.filePath = arg;
			else if (params.output.filePath == NULL) params.output.filePath = arg;
			else usage();
		} else {
			arg += 1;
			if (arg[0] == 'f' || !strcmp(arg, "-file")) {
				if (++i == argc) MissingArgument();
				params.output.fileType = Parse4CharCode(argv[i], "-f | --file");
			} else if (arg[0] == 'd' || !strcmp(arg, "-data")) {
				if (++i == argc) MissingArgument();
				if (!ParseStreamDescription(argv[i], params.output.dataFormat))
					usage();
				gotOutDataFormat = true;
			} else if (arg[0] == 'b' || !strcmp(arg, "-bitrate")) {
				if (++i == argc) MissingArgument();
				params.output.bitRate = ParseInt(argv[i], "-b | --bitrate");
			} else if (arg[0] == 'q' || !strcmp(arg, "-quality")) {
				if (++i == argc) MissingArgument();
				params.output.codecQuality = ParseInt(argv[i], "-q | --quality");
			} else if (arg[0] == 'r' || !strcmp(arg, "-src-quality")) {
				if (++i == argc) MissingArgument();
				params.output.srcQuality = ParseInt(argv[i], "-r | --src-quality");
			} else if (!strcmp(arg, "-src-complexity")) {
				if (++i == argc) MissingArgument();
				params.output.srcComplexity = Parse4CharCode(argv[i], "--src-complexity");
			} else if (arg[0] == 'l' || !strcmp(arg, "-channellayout")) {
				if (++i == argc) MissingArgument();
				UInt32 tag = CAChannelLayouts::StringToConstant(argv[i]);
				if (tag == CAChannelLayouts::kInvalidTag) {
					fprintf(stderr, "unknown channel layout tag: %s\n\n", argv[i]);
					usage();
				}
				if (params.output.channelLayoutTag == 0)
					params.output.channelLayoutTag = tag;
				else if (params.input.channelLayoutTag == 0) {
					params.input.channelLayoutTag = params.output.channelLayoutTag;
					params.output.channelLayoutTag = tag;
				} else {
					fprintf(stderr, "too many channel layout tags!\n\n");
					usage();
				}
			} else if (!strcmp(arg, "-no-filler")) {
				params.output.creationFlags |= kAudioFileFlags_DontPageAlignAudioData;
			} else if (arg[0] == 'c' || !strcmp(arg, "-channels")) {
				if (++i == argc) MissingArgument();
				params.output.channels = ParseInt(argv[i], "-c | --channels");
			} else if (arg[0] == 'v' || !strcmp(arg, "-verbose")) {
				params.flags |= CAAudioFileConverter::kOpt_Verbose;
			} else if (arg[0] == 'h' || !strcmp(arg, "-help")) {
				usage();
			} else if (arg[0] == 's' || !strcmp(arg, "-strategy")) {
				if (++i == argc) MissingArgument();
				params.output.strategy = ParseInt(argv[i], "-s | --strategy");
			} else if (arg[0] == 't' || !strcmp(arg, "-tag")) {
				params.flags |= CAAudioFileConverter::kOpt_CAFTag;
			} else if (!strcmp(arg, "-prime-method")) {
				if (++i == argc) MissingArgument();
				params.output.primeMethod = ParseInt(argv[i], "-p | --prime-method");
			} else if (arg[0] == 'u' || !strcmp(arg, "-userproperty")) {
				if (userPropIndex >= kMaxUserProps)
				{
					fprintf(stderr, "too many user properties!\n\n");
					usage();
				}
				if (++i == argc) MissingArgument();
				params.output.userProperty[userPropIndex].propertyID = Parse4CharCode(argv[i], "-u | --userproperty");
				if (++i == argc) MissingArgument();
				params.output.userProperty[userPropIndex].propertyValue = ParseInt(argv[i], "-u | --userproperty");
				++userPropIndex;
			} else if (!ParseOtherOption(argv, i)) {
				fprintf(stderr, "unknown argument: %s\n\n", arg - 1);
				usage();
			}
		}
	}
	if (params.input.filePath == NULL) {
		fprintf(stderr, "no input file specified\n\n");
		usage();
	}
	
	if (!(params.flags & CAAudioFileConverter::kOpt_CAFTag)) {
		if (!gotOutDataFormat) {
			if (params.output.fileType == 0) {
				fprintf(stderr, "no output file or data format specified\n\n");
				usage();
			}
			if (!CAAudioFileFormats::Instance()->InferDataFormatFromFileFormat(params.output.fileType, params.output.dataFormat)) {
				fprintf(stderr, "Couldn't infer data format from file format\n\n");
				usage();
			}
		} else if (params.output.fileType == 0) {
			CAAudioFileFormats *formats = CAAudioFileFormats::Instance();
			if (!formats->InferFileFormatFromFilename(params.output.filePath, params.output.fileType) && !formats->InferFileFormatFromDataFormat(params.output.dataFormat, params.output.fileType)) {
				params.output.dataFormat.PrintFormat(stderr, "", "Couldn't infer file format from data format");
				usage();
			}
		}
	}

	try {
		mAFConverter->ConvertFile(params);
		Success();
	}
	catch (CAXException &e) {
		char buf[256];
		fprintf(stderr, "Error: %s (%s)\n", e.mOperation, e.FormatError(buf));
		return 1;
	}
	catch (...) {
		fprintf(stderr, "An unknown error occurred\n");
		return 1;
	}

	return 0;
}
