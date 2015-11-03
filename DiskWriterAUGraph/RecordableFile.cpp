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
/*=============================================================================
	RecordableFile.cpp

=============================================================================*/

#include <unistd.h>

#include "RecordableFile.h"


#pragma mark ____CONSTRUCTOR/DESTRUCTOR____

RecordableFile::RecordableFile (const char *inDirectory, const char *inFileName, AudioStreamBasicDescription *inInputStreamFormat)
	:	mFileID (0), 
		mThread (NULL), 
		mGuard ("RecordableFile"), 
		mThreadPriority (0), 
		mThreadHasMoreWorkToDo (false), 
		mThreadShouldDie (false), 
		mThreadIsDead (false), 
		mWriteBuffer (NULL), 
		mBufferPointer (0), 
		mDiskWriteHead (0), 
		mUserWriteHead (0), 
		mFileStreamFormat (),
		mInputStreamFormat (), 
		mAudioConverter (NULL), 
		mConversionBuffer (NULL), 
		mConversionDataRateFactor (0) 
{
	FSRef	directoryRef;
	OSStatus result = noErr;
	
    printf ("created file: %s%s\n", inDirectory, inFileName);
    
	result = FSPathMakeRef ((const UInt8 *)inDirectory, &directoryRef, NULL);
		THROW_RESULT("FSPathMakeRef")
    
	CFStringRef fileNameRef = CFStringCreateWithCStringNoCopy (NULL, inFileName, CFStringGetSystemEncoding(), NULL);
	
	mInputStreamFormat.mSampleRate =		inInputStreamFormat->mSampleRate;
	mInputStreamFormat.mFormatID =			inInputStreamFormat->mFormatID;
	mInputStreamFormat.mFormatFlags =		inInputStreamFormat->mFormatFlags;
	mInputStreamFormat.mBytesPerPacket =	inInputStreamFormat->mBytesPerPacket;
	mInputStreamFormat.mFramesPerPacket =	inInputStreamFormat->mFramesPerPacket;
	mInputStreamFormat.mBytesPerFrame =		inInputStreamFormat->mBytesPerFrame;
	mInputStreamFormat.mChannelsPerFrame =	inInputStreamFormat->mChannelsPerFrame;
	mInputStreamFormat.mBitsPerChannel =	inInputStreamFormat->mBitsPerChannel;
    
	mFileStreamFormat.mSampleRate =			mInputStreamFormat.mSampleRate;
	mFileStreamFormat.mFormatID =			kAudioFormatLinearPCM;
	mFileStreamFormat.mFormatFlags =		kLinearPCMFormatFlagIsSignedInteger
											| kLinearPCMFormatFlagIsPacked
											| kLinearPCMFormatFlagIsBigEndian;
	mFileStreamFormat.mBytesPerPacket =		2;
	mFileStreamFormat.mFramesPerPacket =	1;
	mFileStreamFormat.mBytesPerFrame =		2;
	mFileStreamFormat.mChannelsPerFrame =	1;
	mFileStreamFormat.mBitsPerChannel =		16;
	
	result = AudioFileCreate	(	&directoryRef,
									fileNameRef,
									kAudioFileAIFFType,
									&mFileStreamFormat,
									0,
									NULL,
									&mFileID	);
	
	if (result != noErr) {
		// file probably already exists: try initializing it.
		char*	directoryAndFile = new char[1024];
		FSRef	dirAndFileRef;
		strcpy (directoryAndFile, inDirectory);
		strcat (directoryAndFile, inFileName);
		
		result = FSPathMakeRef ((const UInt8 *)directoryAndFile, &dirAndFileRef, NULL);
			THROW_RESULT("FSPathMakeRef")
		
		result = AudioFileInitialize	(	&dirAndFileRef,
											kAudioFileAIFFType,
											&mFileStreamFormat,
											0,
											&mFileID	);
			THROW_RESULT("AudioFileInitialize")
	}
    
    mConversionDataRateFactor = (Float32)mFileStreamFormat.mBytesPerPacket / (Float32)mInputStreamFormat.mBytesPerPacket;
    
	// create AudioConverter
	result = AudioConverterNew (&mInputStreamFormat, &mFileStreamFormat, &mAudioConverter);
		THROW_RESULT("AudioConverterNew")
	
	// allocate write & conversion buffer
	mWriteBuffer = (char*)malloc (kChunkSize * 2);
	mConversionBuffer = malloc (kChunkSize);
	
	// create & start thread
	mThreadPriority = CAPThread::kMaxThreadPriority;
	mThread = new CAPThread (DiskWriterEntry, this, mThreadPriority);
	mThread->Start();
}

RecordableFile::~RecordableFile ()
{
	OSStatus result = noErr;
	
	if (mWriteBuffer)
	{
		free (mWriteBuffer);
		mWriteBuffer = NULL;
	}
	
	if (mConversionBuffer)
	{
		free (mConversionBuffer);
		mConversionBuffer = NULL;
	}
	
	if (mAudioConverter) {
		result = AudioConverterDispose (mAudioConverter);
			THROW_RESULT("AudioConverterDispose")
		mAudioConverter = 0;
	}
}

#pragma mark ____PUBLIC METHODS____
void	RecordableFile::WriteAudioBuffer (AudioBuffer* inBuffer)
{	
	UInt32 offsetInBuffer = mUserWriteHead % (kChunkSize * 2);
	UInt32 bytesToWrite = inBuffer->mDataByteSize;
    
    // wait until it's safe to write...
    while ( !(((mUserWriteHead + bytesToWrite) - ( (Float32)mDiskWriteHead / mConversionDataRateFactor) ) <= (kChunkSize * 2)) ) {
        usleep (1000);
    }
    
	char* copyLocation = mWriteBuffer + offsetInBuffer;
    
	// copy data into raw buffer
    if ( (bytesToWrite + offsetInBuffer) > (kChunkSize * 2) ) {
        int firstWriteByteCount = (kChunkSize * 2) - offsetInBuffer;
        memcpy (copyLocation, inBuffer->mData, firstWriteByteCount);
        memcpy (mWriteBuffer, (char *)(inBuffer->mData) + firstWriteByteCount, bytesToWrite - firstWriteByteCount);
    } else {
        memcpy (copyLocation, inBuffer->mData, bytesToWrite);
    }
    
	// do we need to wake the writer thread?
	UInt32 startBuffer = (offsetInBuffer / kChunkSize);
	UInt32 endBuffer = ( (offsetInBuffer + bytesToWrite) / kChunkSize);
	
	if (startBuffer != endBuffer) {
		mThreadHasMoreWorkToDo = true;
		mGuard.Notify();
	}
	
	mUserWriteHead += bytesToWrite;
}

void	RecordableFile::Flush ()
{
	OSStatus	result = noErr;
	
	// kill file-writer thread
	mThreadShouldDie = true;
	mGuard.Notify();
	while (!mThreadIsDead) {	// wait until it's dead
        usleep (1000);
    }
	
	// convert last chunk
	char* bufferToConvertFrom = mWriteBuffer + (mBufferPointer * kChunkSize);
	UInt32 finalChunkSize = mUserWriteHead % kChunkSize;
    
    UInt32 dataSize = (UInt32) ((finalChunkSize * mConversionDataRateFactor) + 0.5);
    if ((finalChunkSize > 0) && (dataSize > 0)) {
        result = WriteData (bufferToConvertFrom, dataSize);
            THROW_RESULT("WriteData")
    }
}

void	RecordableFile::Close ()
{	
	OSStatus result = AudioFileClose (mFileID);
		THROW_RESULT("AudioFileClose")
}

#pragma mark ____PRIVATE METHODS____

void * RecordableFile::DiskWriterEntry (void *inRefCon)
{
	RecordableFile* THIS = (RecordableFile*) inRefCon;
	THIS->WriteNextBlock ();
	
	return NULL;
}

void RecordableFile::WriteNextBlock ()
{
	OSStatus	result = noErr;
    CAGuard::Locker	fileWriteLock (mGuard);
    
	while (true)
	{
		if (mThreadShouldDie) {
			mThreadIsDead = true;
			break;	// kill thread
		}
		// put thread in wait state
		while ((!mThreadHasMoreWorkToDo) && !mThreadShouldDie) {
			fileWriteLock.Wait ();
		}
		if (mThreadShouldDie) {
			mThreadIsDead = true;
			break;	// kill thread
		}
		mThreadHasMoreWorkToDo = false;
		
		// convert chunk's format
		UInt32 dataSize = UInt32((kChunkSize * mConversionDataRateFactor) + 0.5);
		char *bufferToConvertFrom = mWriteBuffer + (mBufferPointer * kChunkSize);
        
		result = WriteData (bufferToConvertFrom, dataSize);
        
		if (result) {
			printf ("Result: 0x%lX\n", result);
			printf ("Error trying to convert chunk or write!\n");
			break;
		}
	}
}

OSStatus 	RecordableFile::WriteData (char* inSrcBuffer, UInt32 inDataSize)
{
    OSStatus result;
    result = AudioConverterConvertBuffer (	mAudioConverter,
                                            kChunkSize,
                                            inSrcBuffer,
                                            &inDataSize,
                                            mConversionBuffer	);
    
        if (result) return result;
    
    // write bytes to file		
    result = AudioFileWriteBytes (	mFileID,
                                    false,
                                    mDiskWriteHead,
                                    &inDataSize,
                                    mConversionBuffer	);
        if (result) return result;
    
    mDiskWriteHead += inDataSize;			// increment count
    mBufferPointer = 1 - mBufferPointer;	// switch buffers

    return result;
}
