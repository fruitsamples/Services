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
	afinfox.cpp
	
	Copyright (c) 2006 Apple Computer, Inc.  All Rights Reserved
	$NoKeywords: $
=============================================================================*/

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <AudioToolbox/CAFFile.h>
	#include <AudioToolbox/AudioToolbox.h>
#else
	#include <CAFFile.h>
	#include <AudioToolbox.h>
#endif

#if TARGET_OS_WIN32
	#include <QTML.h>
#endif

#include <stdio.h>
#include "CAStreamBasicDescription.h"
#include <stdlib.h>

void	PrintMarkerList(const char *indent, const AudioFileMarker *markers, int nMarkers);

int main (int argc, const char * argv[]) 
{
#if TARGET_OS_WIN32
	InitializeQTML(0L);
#endif
	
	for (int i=1; i<argc; ++i)
	{
		OSStatus err;
		CAStreamBasicDescription asbd;
		AudioChannelLayout *acl = 0;
		UInt32 propertySize;
		UInt32 specifierSize;
		AudioFileID afid;
		AudioFileTypeID filetype;
		
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)argv[i], strlen(argv[i]), false);
		if (!url) {
			fprintf(stderr, "Can't create CFURL for '%s'\n", argv[i]);
			goto Bail3;
		}
#if 1		
		FSRef fsRef;
		Boolean res = CFURLGetFSRef(url, &fsRef);
		if (!res) {
			fprintf(stderr, "Can't get FSRef for '%s'\n", argv[i]);
			goto Bail2;
		}

		err = AudioFileOpen(&fsRef, fsRdPerm, 0, &afid);
#else
		err = AudioFileOpenURL(url, fsRdPerm, 0, &afid);
#endif

		if (err) {
			fprintf(stderr, "AudioFileOpen failed for '%s'\n", argv[i]);
			goto Bail2;
		}

		propertySize = sizeof(AudioFileTypeID);
		err = AudioFileGetProperty(afid, kAudioFilePropertyFileFormat, &propertySize, &filetype);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyFileFormat failed for '%s'\n", argv[i]);
			goto Bail1;
		}
		filetype = EndianU32_NtoB(filetype);	// for display purposes
		
		propertySize = sizeof(asbd);
		err = AudioFileGetProperty(afid, kAudioFilePropertyDataFormat, &propertySize, &asbd);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyDataFormat failed for '%s'\n", argv[i]);
			goto Bail1;
		}

		printf("File:           %s\n", argv[i]);
		printf("File type ID:   %-4.4s\n", (char *)&filetype);
		asbd.PrintFormat(stdout, "", "Data format:  ");

		printf("\n************************************************************\n");
		UInt32	srcMaxPacketSize,
				size,
				desiredPackets,
				packetBufferSize,
				srcCookieSize;
		UInt64 srcTotalPackets;
		CFDictionaryRef srcInfoDictionary;
		void* srcCookie;
		// Get Packet Size if vbr
		if (asbd.mBytesPerPacket != 0)
			srcMaxPacketSize = asbd.mBytesPerPacket;
		else {
			size = sizeof(srcMaxPacketSize);
			err = AudioFileGetProperty(afid, kAudioFilePropertyMaximumPacketSize, &size, &srcMaxPacketSize);
			if (err) {
				fprintf(stderr, "Could Not get Input file maximum packet size");
				goto Bail1;
			}
		}
		packetBufferSize = 1024 * 64;	
		desiredPackets = packetBufferSize / srcMaxPacketSize;	// try and get thos many packets on each read

		UInt64 dataByteCount;
		propertySize = sizeof(dataByteCount);
		err = AudioFileGetProperty(afid, kAudioFilePropertyAudioDataByteCount, &propertySize, &dataByteCount);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyAudioDataByteCount failed for '%s'\n", argv[i]);
		} else {
			printf("audio bytes:\t%llu\n", dataByteCount);
		}
		
		// Get Source:	 kAudioFilePropertyAudioDataPacketCount
		size = sizeof(srcTotalPackets);
		err = AudioFileGetProperty(afid, kAudioFilePropertyAudioDataPacketCount, &size, &srcTotalPackets);
		printf ("src pkt count:\t%qd\n", srcTotalPackets);
		UInt64 dataPacketCount;
		UInt64 totalFrames;
		totalFrames = 0;
		propertySize = sizeof(dataPacketCount);
		err = AudioFileGetProperty(afid, kAudioFilePropertyAudioDataPacketCount, &propertySize, &dataPacketCount);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyAudioDataPacketCount failed for '%s'\n", argv[i]);
		} else {
			printf("audio packets:\t%llu\n", dataPacketCount);
			if (asbd.mFramesPerPacket)
				totalFrames = asbd.mFramesPerPacket * dataPacketCount;
		}
		
		AudioFilePacketTableInfo pti;
		propertySize = sizeof(pti);
		err = AudioFileGetProperty(afid, kAudioFilePropertyPacketTableInfo, &propertySize, &pti);
		if (err == noErr) {
			totalFrames = pti.mNumberValidFrames;
			printf("total frames:\t%qd (%qd valid + %d priming + %d remainder)\n",  pti.mNumberValidFrames + pti.mPrimingFrames + pti.mRemainderFrames, pti.mNumberValidFrames, (unsigned)pti.mPrimingFrames, (unsigned)pti.mRemainderFrames);
		}
		
		if (totalFrames != 0)
			printf("duration:\t%.4f seconds\n", (double)totalFrames / (double)asbd.mSampleRate );

		UInt32 maxPacketSize;
		propertySize = sizeof(maxPacketSize);
		err = AudioFileGetProperty(afid, kAudioFilePropertyMaximumPacketSize, &propertySize, &maxPacketSize);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyMaximumPacketSize failed for '%s'\n", argv[i]);
		} else {
			printf("max pkt size:\t%lu\n", maxPacketSize);
		}

		#define kAudioFilePropertyPacketSizeUpperBound 'pkub'
		UInt32 packetSizeUpperBound;
		propertySize = sizeof(packetSizeUpperBound);
		err = AudioFileGetProperty(afid, kAudioFilePropertyPacketSizeUpperBound, &propertySize, &packetSizeUpperBound);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyPacketSizeUpperBound failed for '%s'\n", argv[i]);
		} else {
			printf("packet size upper bound: %lu\n", packetSizeUpperBound);
		}
		
		if (packetSizeUpperBound < maxPacketSize) printf("!!!!!!!!!! packetSizeUpperBound < maxPacketSize\n");
		
		UInt64 dataOffset;
		propertySize = sizeof(dataOffset);
		err = AudioFileGetProperty(afid, kAudioFilePropertyDataOffset, &propertySize, &dataOffset);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyDataOffset failed for '%s'\n", argv[i]);
		} else {
			printf("audio data file offset: %llu\n", dataOffset);
		}
		
		// Get the Source:	 kAudioFilePropertyMagicCookieData
		srcCookieSize = 0;
		err = AudioFileGetPropertyInfo(afid, kAudioFilePropertyMagicCookieData, &srcCookieSize, NULL);
		if (err == noErr && srcCookieSize > 0) {
			srcCookie = calloc(1, srcCookieSize);
			if (srcCookie) {
				err = AudioFileGetProperty(afid, kAudioFilePropertyMagicCookieData, &srcCookieSize, srcCookie);
				if (err == noErr)
					printf ("source has cookie: TRUE\n");
			}
		}
		else if (err == noErr) printf ("Source Has Cookie: FALSE\n");
		else printf("Error obtaining Cookie information (%d)\n", err);

		printf("\n************************************************************\n");
		// Get Source:	 kAudioFilePropertyInfoDictionary
		size = 0;
		err = AudioFileGetPropertyInfo(afid, kAudioFilePropertyInfoDictionary, &size, NULL);
		if (err == noErr) {
			err = AudioFileGetProperty(afid, kAudioFilePropertyInfoDictionary, &size, &srcInfoDictionary);
			if (err == noErr)
				printf ("Source Has Info Dictionary: TRUE\n");
		}
		else
			printf ("Source Has Info Dictionary: FALSE\n");
			
		if (srcInfoDictionary)
		{
			// print the contents of the dictionary
			CFShow(srcInfoDictionary);
		}
			
		printf("\n************************************************************\n");
		UInt32 writable;
		UInt32 isOptimized;
		propertySize = sizeof(isOptimized);
		err = AudioFileGetProperty(afid, kAudioFilePropertyIsOptimized, &propertySize, &isOptimized);
		if (err) {
			fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyIsOptimized failed for '%s'\n", argv[i]);
		} else {
			printf(isOptimized ? "optimized\n" : "not optimized\n");
		}

		err = AudioFileGetPropertyInfo(afid, 'flst'/*kAudioFilePropertyFormatList*/, &propertySize, &writable);
		if (err) {
			// this should never happen with an AudioToolbox that has this property defined.
			fprintf(stderr, "AudioFileGetPropertyInfo kAudioFilePropertyFormatList failed for '%s'\n", argv[i]);
		} else {
			struct	TEMP_AudioFormatListItem {
				AudioStreamBasicDescription		mASBD;
				AudioChannelLayoutTag			mChannelLayoutTag;
			};
			
			TEMP_AudioFormatListItem*	formatList = NULL;
			AudioChannelLayout		tLayout;

			formatList = (TEMP_AudioFormatListItem*) calloc (1, propertySize);
			if (formatList)
			{
				err = AudioFileGetProperty(afid, 'flst'/*kAudioFilePropertyFormatList*/, &propertySize, formatList);
				if (err) {
					fprintf(stderr, "AudioFileGetProperty kAudioFilePropertyFormatList failed for '%s'\n", argv[i]);
				}
				else
				{
					// the returned list may actually be smaller than GetPropertyInfo returned
					UInt32		actualListCount = propertySize/sizeof(TEMP_AudioFormatListItem);;
					//printf("FormatList: formats available: %ld\n", actualListCount);
					printf("format list:\n");
					tLayout.mChannelBitmap = 0;
					tLayout.mNumberChannelDescriptions = 0;

					for (UInt32	i = 0; i < actualListCount; i++)
					{
						//printf("FormatList index: %ld\n", i+1);
						char label[32];
						sprintf(label, "[%2ld] format:   ", i);
						// ----- print the format ASBD
						asbd = formatList[i].mASBD;
						asbd.PrintFormat(stdout, "", label);
						
						// ----- print the channel layout name
						CFStringRef layoutName;
						char aclstr[512];
						tLayout.mChannelLayoutTag = formatList[i].mChannelLayoutTag;
						
						if ((tLayout.mChannelLayoutTag & 0xFFFF0000) == 0xFFFF0000) {
							// unknown/unspecified layout
							printf("                no channel layout.\n");
						} else {
							specifierSize = sizeof(tLayout);
							propertySize = sizeof(layoutName);
							err = AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutName, specifierSize, (void*) &tLayout, &propertySize, &layoutName);
							if (err) {
								fprintf(stderr, "Could not get the ChannelLayout name for this format list item\n");
							}
							else
							{
								CFStringGetCString(layoutName, aclstr, 512, kCFStringEncodingUTF8);
								CFRelease(layoutName);
								printf("     Channel layout: %s\n", aclstr);
							}
						}
					}
				}
			}
			
			else printf ("Source Has Channel Layout: FALSE\n");
		}
		
		printf("\n************************************************************\n");
		// regions
		err = AudioFileGetPropertyInfo(afid, kAudioFilePropertyRegionList, &propertySize, NULL);
		if ((err == noErr) && (propertySize > 0)) {
			AudioFileRegionList *regionList = (AudioFileRegionList *)malloc(propertySize);
			err = AudioFileGetProperty(afid, kAudioFilePropertyRegionList, &propertySize, regionList);
			if (err == noErr) {
				printf("%d regions; SMPTE time type %d\n", int(regionList->mNumberRegions), int(regionList->mSMPTE_TimeType));
				AudioFileRegion *rgn = regionList->mRegions;
				for (UInt32 i = 0; i < regionList->mNumberRegions; ++i) {
					char name[512];
					CFStringGetCString(rgn->mName, name, sizeof(name), kCFStringEncodingUTF8);
					CFRelease(rgn->mName);
					printf("  region %d, \"%s\", flags %08X\n", int(rgn->mRegionID), name, int(rgn->mFlags));
					PrintMarkerList("    ", rgn->mMarkers, rgn->mNumberMarkers);
					rgn = NextAudioFileRegion(rgn);
				}
			}
			free(regionList);
		}

		else printf ("Source Has Region List: FALSE\n");	
			
		printf("\n************************************************************\n");
		// markers
		err = AudioFileGetPropertyInfo(afid, kAudioFilePropertyMarkerList, &propertySize, NULL);
		if (err == noErr && propertySize > 0) {
			AudioFileMarkerList *markerList = (AudioFileMarkerList *)malloc(propertySize);
			err = AudioFileGetProperty(afid, kAudioFilePropertyMarkerList, &propertySize, markerList);
			if (err == noErr) {
				printf("%d markers; SMPTE time type %d\n", int(markerList->mNumberMarkers), int(markerList->mSMPTE_TimeType));
				PrintMarkerList("  ", markerList->mMarkers, markerList->mNumberMarkers);
			}
			free(markerList);
		}
		
		printf("\n************************************************************\n");
		printf("Chunks IDs:\n");
		UInt32 iosize;
		err = AudioFileGetPropertyInfo(afid, kAudioFilePropertyChunkIDs, &iosize, &writable);
		if (err) printf("AudioFileGetPropertyInfo kAudioFilePropertyChunkIDs %d '%4.4s'\n", err, &err);
		//tassert2(!err, "AudioFileGetPropertyInfo kAudioFilePropertyChunkIDs");	
		UInt32 *ids = (UInt32*)malloc(iosize);
		memset(ids, 'x', iosize);
		
		err = AudioFileGetProperty(afid, kAudioFilePropertyChunkIDs, &iosize, ids);
		if (err) printf("AudioFileGetProperty kAudioFilePropertyChunkIDs %d '%4.4s'\n", err, &err);
		//tassert2(!err, "AudioFileGetProperty kAudioFilePropertyChunkIDs");	

		for (UInt32 i=0; i<iosize/sizeof(UInt32); ++i)
		{
			UInt32 temp = EndianU32_NtoB(*(ids+i));
			printf("%d '%4.4s'\n", i, &temp);
		}
		
		Bail1:
		AudioFileClose(afid);
		Bail2:
		CFRelease(url);
		Bail3:
		free(acl);
		
		printf("----\n");
	}
    return 0;
}

// N.B. releases the marker names
void	PrintMarkerList(const char *indent, const AudioFileMarker *markers, int nMarkers)
{
	const AudioFileMarker *mrk = markers;
	for (int i = 0; i < nMarkers; ++i, ++mrk) {
		char name[512];
		CFStringGetCString(mrk->mName, name, sizeof(name), kCFStringEncodingUTF8);
		printf("%smarker %d, \"%s\": frame %9.0f   SMPTE time %d:%02d:%02d:%02d/%d  type %d  channel %d\n",
			indent, int(mrk->mMarkerID), name, mrk->mFramePosition, 
			mrk->mSMPTETime.mHours, mrk->mSMPTETime.mMinutes, mrk->mSMPTETime.mSeconds, mrk->mSMPTETime.mFrames, 
				int(mrk->mSMPTETime.mSubFrameSampleOffset),
			int(mrk->mType), int(mrk->mChannel));
		CFRelease(mrk->mName);
	}
}


