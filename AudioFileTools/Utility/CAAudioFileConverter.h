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
	CAAudioFileConverter.h
	
=============================================================================*/

#ifndef __CAAudioFileConverter_h__
#define __CAAudioFileConverter_h__

#include <AudioToolbox/AudioToolbox.h>
#include "CAStreamBasicDescription.h"
#include "CABufferList.h"
#include "CAHostTimeBase.h"
#include "CAAudioFile.h"

class CAAudioFileConverter {
public:
	// options for ConversionParameters.flags
	enum {
		kOpt_OverwriteOutputFile	= 1,
		kOpt_Profile				= 2,		// collect performance profiling info
		kOpt_Verbose				= 4
	};
	
	struct ConversionParameters {
		ConversionParameters();
		
		UInt32					flags;
		
		struct {
			const char *			filePath;			// POSIX path, UTF-8
			AudioFileID				audioFileID;		// or set this to non-zero
			AudioChannelLayoutTag	channelLayoutTag;	// 0 to use the one in the file, or default
		}						input;
		
		struct {
			const char *			filePath;			// POSIX path; if null, file is written to same
														// directory as input file, name generated from
														// its name with the appropriate filename extension
														// (fails if file already exists)
			OSType					fileType;
			CAStreamBasicDescription dataFormat;
			int						channels;			// -1 for same number of channels as input
			SInt32					bitRate;			// -1 for default
			SInt32					quality;			// 0-127, -1 for default
			AudioChannelLayoutTag	channelLayoutTag;	// 0 for default
		}						output;
		
		//UInt32					bufferSize;				// [UNIMPLEMENTED]
	};
	
	CAAudioFileConverter();

	virtual ~CAAudioFileConverter();
	
	void			ConvertFile(const ConversionParameters &params);
	virtual bool	Progress(SInt64 packetsConverted, SInt64 packetsTotal) { return false; }
	virtual void	GenerateOutputFileName(const char *inputFilePath, 
						const CAStreamBasicDescription &inputFormat,
						const CAStreamBasicDescription &outputFormat,
						OSType outputFileType, char *outName);
	
	const char *	GetOutputFileName() { return mOutName; }
	const CAAudioFile &	InputFile() { return mSrcFile; }
	const CAAudioFile & OutputFile() { return mDestFile; }
	
	// performance
#if CAAUDIOFILE_PROFILE
	UInt64			TicksInDecode() { return mSrcFile.TicksInConverter(); }
	UInt64			TicksInEncode() { return mDestFile.TicksInConverter(); }
	UInt64			TicksInRead() { return mSrcFile.TicksInIO(); }
	UInt64			TicksInWrite() { return mDestFile.TicksInIO(); }
#endif
	
private:
	void						PrintFormats(const CAAudioChannelLayout *origSrcFileLayout);

	UInt64						CurrentTime() {
									if (mParams->flags & kOpt_Profile)
										return CAHostTimeBase::GetCurrentTimeInNanos();
									return 0;
								}
	
	const ConversionParameters *mParams;
	AudioConverterRef			mConverter;
	CAAudioFile					mSrcFile;
	CAAudioFile					mDestFile;
	
	CABufferList *				mReadBuffer;
	CABufferList *				mReadPtrs;
protected:
	char						mOutName[256];
};

#endif // __CAAudioFileConverter_h__
