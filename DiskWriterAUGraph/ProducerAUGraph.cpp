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
	ProducerAUGraph.cpp

=============================================================================*/

#define MIX(A,B) (A < B) ? A : B
#define MAX(A,B) (A > B) ? A : B

#include <unistd.h>

#include "ProducerAUGraph.h"

#pragma mark ____ Constructor & Setup ____
ProducerAUGraph::ProducerAUGraph (const char* inMIDIFilename, const char * inBaseWriteDirectory, const char * inBaseOutputFilename) :
    mIsRunningCallCount (0),
    mProcessorThread (NULL),
    mRenderingStarted (FALSE)
{
    OSErr result;
    UInt32 dataSize;
    
    // setup graph
    result = NewAUGraph (&mAUGraph);
        THROW_RESULT ("NewAUGraph");
    result = AUGraphOpen (mAUGraph);
        THROW_RESULT ("AUGraphOpen");
    result = AUGraphInitialize (mAUGraph);
        THROW_RESULT ("AUGraphInitialize");
    
    _BuildGraph ();
    
    // create files (1 for each channel in output unit's output stream: generally ends up being stereo)
    dataSize = sizeof (AudioStreamBasicDescription);
    mOutputStreamDescription = (AudioStreamBasicDescription *) malloc (dataSize);
    result = AudioUnitGetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, mOutputStreamDescription, &dataSize);
        THROW_RESULT ("AudioUnitGetProperty");
    
    mRecordableFile = (RecordableFile**) malloc ( mOutputStreamDescription->mChannelsPerFrame * sizeof (RecordableFile*) );
    
    char	filename[64];
    for (UInt32 i = 0; i < mOutputStreamDescription->mChannelsPerFrame; i++) {
        sprintf (filename, "%s_Channel_%lu.aif", inBaseOutputFilename, i);
        mRecordableFile[i] = new RecordableFile (inBaseWriteDirectory, filename, mOutputStreamDescription);
    }
    
    _CreateSequence (inMIDIFilename);
    result = MusicSequenceSetAUGraph (mMusicSequence, mAUGraph);
        THROW_RESULT ("MusicSequenceSetAUGraph");
    
    // create thread
	mProcessorThread = new CAPThread (ProductionThreadRouter, this, CAPThread::kMaxThreadPriority);
    
    result = MusicPlayerStart (mMusicPlayer);
        THROW_RESULT ("MusicPlayerStart");
}

ProducerAUGraph::~ProducerAUGraph ()
{
    OSErr result;
    // dispose player & sequence
    result = DisposeMusicPlayer (mMusicPlayer);
        THROW_RESULT ("DisposeMusicPlayer");
    result = DisposeMusicSequence (mMusicSequence);
        THROW_RESULT ("DisposeMusicSequence");
    
    // dispose graph
    result = AUGraphStop (mAUGraph);
        THROW_RESULT ("AUGraphStop");    
    result = AUGraphUninitialize (mAUGraph);
        THROW_RESULT ("AUGraphUninitialize");
    result = AUGraphClose (mAUGraph);
        THROW_RESULT ("AUGraphClose");
    result = DisposeAUGraph (mAUGraph);
        THROW_RESULT ("DisposeAUGraph");
}

void ProducerAUGraph::_BuildGraph ()
{
    ComponentDescription cd;
    OSStatus result;
    
    // [music device unit]
	cd.componentType = kAudioUnitType_MusicDevice;
	cd.componentSubType = kAudioUnitSubType_DLSSynth;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;        
	cd.componentFlagsMask = 0;     
    
	result = AUGraphNewNode (mAUGraph, &cd, 0, NULL, &mDLSNode);
		THROW_RESULT("AUGraphNewNode: DLSMusicDevice")
    
    result = AUGraphGetNodeInfo (mAUGraph, mDLSNode, NULL, NULL, NULL, &mDLSUnit);
        THROW_RESULT("AUGraphGetNodeInfo");
    
    // [Generic output unit]
	cd.componentType = kAudioUnitType_Output;
	cd.componentSubType = kAudioUnitSubType_GenericOutput;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;
    
	result = AUGraphNewNode (mAUGraph, &cd, 0, NULL, &mOutputNode);
		THROW_RESULT("AUGraphNewNode: Output")
    
    result = AUGraphGetNodeInfo (mAUGraph, mOutputNode, NULL, NULL, NULL, &mOutputUnit);
        THROW_RESULT("AUGraphGetNodeInfo");
    
    // connect nodes
    result = AUGraphConnectNodeInput (mAUGraph, mDLSNode, 0, mOutputNode, 0);
        THROW_RESULT("AUGraphConnectNodeInput");
    result = AUGraphUpdate (mAUGraph, NULL);
        THROW_RESULT("AUGraphUpdate");
    
    // attach property listener
    result = AudioUnitAddPropertyListener (mOutputUnit, kAudioOutputUnitProperty_IsRunning, IsRunningPropertyListenerRouter, this);
        THROW_RESULT("AudioUnitAddPropertyListener");
}

void ProducerAUGraph::_CreateSequence (const char * inMIDIFilename)
{
    OSStatus result;
	FSRef fsRef;
    FSSpec outSpec;
    
    result = NewMusicSequence (&mMusicSequence);
        THROW_RESULT ("NewMusicSequence");
    
	result = FSPathMakeRef ((const UInt8*)inMIDIFilename, &fsRef, 0);
		THROW_RESULT ("FSPathMakeRef");
    
	result = FSGetCatalogInfo(&fsRef, kFSCatInfoNone, NULL, NULL, &outSpec, NULL);
		THROW_RESULT ("FSGetCatalogInfo");
    
    result = MusicSequenceLoadSMF (mMusicSequence, &outSpec);
        THROW_RESULT ("MusicSequenceLoadSMF");
    
    result = NewMusicPlayer (&mMusicPlayer);
        THROW_RESULT ("NewMusicPlayer");
    
    result = MusicPlayerSetSequence (mMusicPlayer, mMusicSequence);
        THROW_RESULT ("MusicPlayerSetSequence");
}

#pragma mark ____ Public Accessors ____
Boolean ProducerAUGraph::IsRunning ()
{
    OSStatus	result;
    Boolean		isRunning;
    
    result = AUGraphIsRunning (mAUGraph, &isRunning);
        THROW_RESULT ("AUGraphIsRunning");
    
    return isRunning;
}

Float32	ProducerAUGraph::GetPercentComplete ()
{
    if (mRenderingStarted) {
        return ((Float32)mCurrentSample / (Float32)mSongLengthInSamples);
    }
    
    return 0;
}

#pragma mark ____ Property Monitor ____
void ProducerAUGraph::IsRunningPropertyListenerRouter (void *inRefCon, AudioUnit inAU, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement)
{
    ProducerAUGraph *THIS = (ProducerAUGraph *)inRefCon;
    THIS->IsRunningPropertyListener (inAU, inID, inScope, inElement);
}

void ProducerAUGraph::IsRunningPropertyListener (AudioUnit inAU, AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement)
{
    mIsRunningCallCount++;
    
    if (mIsRunningCallCount == 1) {
        mProcessorThread->Start();
    }
}

#pragma mark ____ Data Production ____
void * ProducerAUGraph::ProductionThreadRouter (void * inRefCon)
{
    ProducerAUGraph * THIS = (ProducerAUGraph *) inRefCon;
    THIS->ProductionThreadMain();
    
    return NULL;
}

void ProducerAUGraph::ProductionThreadMain ()
{
    // wait for graph to start running
    if ( !IsRunning() ) {
        while (!IsRunning ()) {
            usleep (20000);
        }
    }
    
    // these values can be adjusted as you like
    UInt32	frameCount = 1024;		// number of frames to render for each call to AudioUnitRender (...)
    UInt64	startingSample = 0;		// sample offset in song to start rendering at.  0 is the beginning,
                                    // which seems like a good place to start.
    
    // allocate render buffers
    AudioBufferList *theBufferList = (AudioBufferList *)malloc (offsetof(AudioBufferList, mBuffers[mOutputStreamDescription->mChannelsPerFrame]));
    theBufferList->mNumberBuffers = mOutputStreamDescription->mChannelsPerFrame;
    for (UInt32 i = 0; i < theBufferList->mNumberBuffers; i++) {
       theBufferList->mBuffers[i].mNumberChannels = 1;
        theBufferList->mBuffers[i].mDataByteSize = frameCount * 4;
        theBufferList->mBuffers[i].mData = NULL;
    }
    
    OSStatus			result;
    AudioTimeStamp		timeStamp;
    UInt32				i;
    Float64				sequenceLengthInSeconds;
    
    mCurrentSample = startingSample;
    
    result = MusicSequenceGetSecondsForBeats (	mMusicSequence,
                                                _SequenceLengthInBeats(),
                                                &sequenceLengthInSeconds	);
        THROW_RESULT ("ProductionThreadMain");
    
    sequenceLengthInSeconds += 2;	// add 2 seconds to end of song to allow for rendering trailing reverb.
                                    // (otherwise we end on the sample immediately following the last event.)
                                    // this value can be adjusted to taste.

    mSongLengthInSamples = (UInt64)(mOutputStreamDescription->mSampleRate * sequenceLengthInSeconds);
    
    // render audio
    mRenderingStarted = TRUE;
    while (mCurrentSample < mSongLengthInSamples)
    {
        // rendering
        timeStamp.mSampleTime = mCurrentSample;
        timeStamp.mFlags = kAudioTimeStampSampleTimeValid;
        
        //get audio slice
        result = AudioUnitRender (mOutputUnit, 0, &timeStamp, 0, frameCount, theBufferList);
            THROW_RESULT ("ProductionThreadMain");
        
        // write it to disk
        for (i = 0; i < mOutputStreamDescription->mChannelsPerFrame; i++) {
            mRecordableFile[i]->WriteAudioBuffer ( &(theBufferList->mBuffers[i]) );
        }
        
        mCurrentSample += frameCount;
    }
    
    // flush & close files
    for (i = 0; i < mOutputStreamDescription->mChannelsPerFrame; i++) {
        mRecordableFile[i]->Flush();
        mRecordableFile[i]->Close();
    }
    
    result = MusicPlayerStop (mMusicPlayer);
        THROW_RESULT ("MusicPlayerStop");
    
    free (theBufferList);
}

#pragma mark ____ Private Utilities ____
MusicTimeStamp ProducerAUGraph::_SequenceLengthInBeats ()
{
    OSStatus				result;
    UInt32					trackCount;
    MusicTrack				currentTrack;
    MusicEventIterator		currentIterator;
    MusicTimeStamp			currentTimeStamp;
    Boolean					hasPrevEvent;
    MusicTimeStamp			maxLength;
    
    maxLength = 0;
    result = MusicSequenceGetTrackCount (mMusicSequence, &trackCount);
        THROW_RESULT ("_SequenceLengthInBeats");
    
    // find the length of each track (in beats), and return the length of the longest track (in beats)
    for (UInt32 i = 0; i < trackCount; ++i) {
        result = MusicSequenceGetIndTrack(mMusicSequence, i, &currentTrack);
            THROW_RESULT ("_SequenceLengthInBeats");
            
        result = NewMusicEventIterator (currentTrack, &currentIterator);
            THROW_RESULT ("_SequenceLengthInBeats");
            
        result = MusicEventIteratorSeek (currentIterator, kMusicTimeStamp_EndOfTrack);
            THROW_RESULT ("_SequenceLengthInBeats");
            
        result = MusicEventIteratorHasPreviousEvent (currentIterator, &hasPrevEvent);
            THROW_RESULT ("_SequenceLengthInBeats");
            
        if (hasPrevEvent) {
            result = MusicEventIteratorPreviousEvent (currentIterator);
                THROW_RESULT ("_SequenceLengthInBeats");
                
            result = MusicEventIteratorGetEventInfo (	currentIterator,
                                                        &currentTimeStamp,
                                                        NULL,
                                                        NULL,
                                                        NULL	);
                THROW_RESULT ("_SequenceLengthInBeats");
                
            maxLength = MAX(currentTimeStamp,maxLength);
        }
    }
    
    return maxLength;
}
