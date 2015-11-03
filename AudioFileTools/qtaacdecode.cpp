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
	qtaacdecode.cpp
	
=============================================================================*/

/*
	Simple example program to extract audio from a QuickTime/MP4 AAC file, and write it
	to an AIFF.
*/

#include <stdlib.h>
#include "QTAACFile.h"
#include "CAAudioFile.h"
#include "CAXException.h"

static void	usage()
{
	fprintf(stderr, "Usage: %s infile outfile\n", getprogname());
	exit(2);
}

int qtaacdecode(const char *infileName, const char *outfileName)
{
	QTAACFile infile;
	CAAudioFile outfile;
	CABufferList *readBuffer = NULL, *readPtrs = NULL;
	bool createdOutputFile = false;
	
	try {
		EnterMovies();
		infile.Open(infileName);
		
		// for now we'll always convert to 16-bit stereo 44100 AIFF
		CAStreamBasicDescription dataFormat;
		dataFormat.mSampleRate = 44100.;
		dataFormat.mFormatID = kAudioFormatLinearPCM;
		dataFormat.mFormatFlags = kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		dataFormat.mFramesPerPacket = 1;
		dataFormat.mChannelsPerFrame = 2;
		dataFormat.mBitsPerChannel = 16;
		dataFormat.mBytesPerPacket = dataFormat.mBytesPerFrame = 4;

		infile.SetClientFormat(dataFormat);
		outfile.PrepareNew(dataFormat);
		
		// create the output file
		outfile.Create(outfileName, kAudioFileAIFFType);
		createdOutputFile = true;
		
		// prepare I/O buffers
		const UInt32 bytesToRead = 0x8000;
		const UInt32 packetsToRead = bytesToRead;	// OK, ReadPackets will limit as appropriate
		
		readBuffer = CABufferList::New("readbuf", dataFormat);
		readBuffer->AllocateBuffers(bytesToRead);
		readPtrs = CABufferList::New("readptrs", dataFormat);
		
		while (true) {
			UInt32 nPackets = packetsToRead;
			readPtrs->SetFrom(readBuffer);
			AudioBufferList *readbuf = &readPtrs->GetModifiableBufferList();
			
			infile.ReadPackets(nPackets, readbuf);
			if (nPackets == 0)
				break;

			outfile.WritePackets(nPackets, readbuf);
		}
	}
	catch (CAXException &e) {
		char buf[256];
		fprintf(stderr, "Error: %s (%s)\n", e.mOperation, CAXException::FormatError(buf, e.mError));
		delete readBuffer;
		delete readPtrs;
		infile.Close();
		if (createdOutputFile)
			outfile.Delete();
		return 1;
	}
	catch (...) {
		fprintf(stderr, "An unknown error occurred\n");
		delete readBuffer;
		delete readPtrs;
		infile.Close();
		if (createdOutputFile)
			outfile.Delete();
		return 1;
	}
	infile.Close();
	outfile.Close();
	return 0;
}

int main(int argc, const char *argv[])
{
	if (argc != 3)
		usage();
	const char *infile = argv[1];
	const char *outfile = argv[2];	
	
	return qtaacdecode(infile, outfile);
}
