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
	playaudiofile.cpp
	
=============================================================================*/

#include "CAChannelMappingPlayer.h"
#include "CAXException.h"
#include <unistd.h>

static void usage()
{
	fprintf(stderr,
			"Usage:\n"
			"%s [option...] audio_file\n\n"
			"Options: (may appear before or after arguments)\n"
			"  {-a --all}\n"
			"    for files with >2 channels, play each pair of channels in succession\n"
			"  {-f --frame}\n"
			"    starting sample frame number\n"
			, getprogname());
	exit(1);
}

static void Play(CAChannelMappingPlayer &player)
{
	player.Start();
	while (true) {
		if (player.GetCurrentPosition() >= 1.0)
			break;
		usleep(250000);	// 250 ms
	}
	player.Stop();
}

int main(int argc, const char *argv[])
{
	const char *fileToPlay = NULL;
	bool playallpairs = false;
	SInt64 startFrame = 0;

	for (int i = 1; i < argc; ++i) {
		const char *arg = argv[i];
		if (arg[0] != '-') {
			if (fileToPlay != NULL) {
				fprintf(stderr, "may only specify one file to play\n");
				usage();
			}
			fileToPlay = arg;
		} else {
			arg += 1;
			if (arg[0] == 'a' || !strcmp(arg, "-all"))
				playallpairs = true;
			else if (arg[0] == 'f' || !strcmp(arg, "-frame")) {
				i += 1;
				arg = argv[i];
				sscanf(arg, "%qd", &startFrame);
			} else {
				fprintf(stderr, "unknown argument: %s\n\n", arg - 1);
				usage();
			}
		}
	}
	
	if (fileToPlay == NULL)
		usage();
	
	try {
		const int kNumberBuffers = 3;
		const unsigned kBufferSize = 0x8000;
		CAAudioFile file;
		file.Open(fileToPlay);
		CAChannelMappingPlayer player(kNumberBuffers, kBufferSize);
		player.SetFile(file.GetAudioFileID());
		CAChannelMapper *mapper = player.GetMapper();
		if (playallpairs && mapper == NULL)
			playallpairs = false;
		
		if (playallpairs) {
			int nChannelsInFile = file.GetFileDataFormat().NumberChannels();
			for (int channel = 0; channel < nChannelsInFile && playallpairs; channel += 2) {
				printf("channels %d-%d of %d\n", channel, channel+1, nChannelsInFile);
				mapper->ResetMixer();
				mapper->ConnectChannelToChannel(channel, 0);
				mapper->ConnectChannelToChannel(channel+1, 1);
				player.SetCurrentPosition(0.);
				Play(player);
			}
		} else {
			player.GetFile().SeekToFrame(startFrame);
			Play(player);
		}
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

