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
#import <AudioUnit/AUCocoaUIView.h>

#import "HostingWindowController.h"
#import "AUEffectWrapper.h"
#import "MessageView.h"
#import "AudioFilePlay.h"


#pragma mark ____STATIC METHODS____
void AudioFileNotificationHandler (void *inRefCon, OSStatus inStatus) {
    HostingWindowController *SELF = (HostingWindowController *)inRefCon;
    [SELF performSelectorOnMainThread:@selector(iaPlayStopButtonPressed:) withObject:SELF waitUntilDone:NO];
}

@implementation HostingWindowController
/*******************************************************************
   Quick and dirty view verification method that returns true if 
   the plugin class conforms to the protocol and responds to all of 
   its methods
 *******************************************************************/
+ (BOOL)plugInClassIsValid:(Class) pluginClass {
	if ([pluginClass conformsToProtocol: @protocol(AUCocoaUIBase)]) {
		if ([pluginClass instancesRespondToSelector: @selector(interfaceVersion)] &&
			[pluginClass instancesRespondToSelector: @selector(uiViewForAudioUnit:withSize:)]) {
			return YES;
		}
	}
	
    return NO;
}

#pragma mark ____INIT/DEALLOC____
- (void)awakeFromNib {
	mEffectList = [[self getEffectsList] retain];
    mAudioFileList = [[NSMutableArray alloc] init];
    
    int i;
	[uiAUPopUpButton removeAllItems];
	for (i = 0; i < [mEffectList count]; i++) {
		[uiAUPopUpButton addItemWithTitle:[[mEffectList objectAtIndex:i] effectName]];
	}
    
    NSRect frameRect;
    frameRect.origin = NSZeroPoint;
    frameRect.size = [[uiAUViewContainer contentView] frame].size;
    mEmptyView = [[MessageView alloc] initWithFrame:frameRect text:@"No view available" proportionalTextSize:1.0f];
	
	[self createGraph];
    [self iaAUPopUpButtonPressed:self];
    [uiPlayStopButton setEnabled:NO];	// initially set disabled because there are no files in the list
}

-(void)dealloc {
    [mEmptyView release];
    
	[mEffectList release];
    [mAudioFileList release];
    
    if (mAFPID) NSAssert(DisposeAudioFilePlayID(mAFPID) == noErr, @"DisposeAudioFilePlayID()");
    
    NSLog (@"need to close out AUGraph here...");
}

/*******************************************************************
   Get a list of the Audio Units that are present on the system and
   place in an NSArray 
 *******************************************************************/
- (NSArray *)getEffectsList {
	NSMutableArray *theEffectsList = [[NSMutableArray alloc] init];

	ComponentDescription desc;
	
	desc.componentFlags 		= 0;        
	desc.componentFlagsMask 	= 0;     
	desc.componentSubType 		= 0;       	
	desc.componentManufacturer 	= 0;
	desc.componentType 			= kAudioUnitType_Effect;	// only show effect types, not music devices
	
	if (CountComponents (&desc) == 0)
		return nil;	// didn't find any components
	
	Component comp = 0;
	
	comp = FindNextComponent (NULL, &desc);
	while (comp != NULL) {
		[theEffectsList addObject:[[AUEffectWrapper alloc] initWithComponent:comp]];
		comp = FindNextComponent (comp, &desc);
	}
	return [theEffectsList autorelease];
}

- (IBAction)iaBypassAUPressed:(id)sender {
    NSLog (@"not yet implemented");
}

- (IBAction)iaAUPopUpButtonPressed:(id)sender {
    // replace effect AU in chain
	int index = [uiAUPopUpButton indexOfSelectedItem];
	ComponentDescription desc = [[mEffectList objectAtIndex:index] componentDescription];
	
	if (mTargetNode) NSAssert(AUGraphRemoveNode(mGraph, mTargetNode) == noErr, @"AUGraphRemoveNode()");
    
    NSAssert(AUGraphNewNode(mGraph, &desc, 0, NULL, &mTargetNode) == noErr, @"AUGraphNewNode()");
	NSAssert(AUGraphConnectNodeInput (mGraph, mTargetNode, 0, mOutputNode, 0) == noErr, @"AUGraphConnectNodeInput()");
	NSAssert(AUGraphGetNodeInfo(mGraph, mTargetNode, NULL, NULL, NULL, &mTargetUnit) == noErr, @"AUGraphGetNodeInfo()");
    NSAssert(AUGraphUpdate (mGraph, NULL) == noErr, @"AUGraphUpdate");
    
	// get AU's Cocoa view property
    UInt32 						dataSize;
    Boolean 					isWritable;
    AudioUnitCocoaViewInfo *	cocoaViewInfo;
    UInt32						numberOfClasses;
    
    OSStatus result = AudioUnitGetPropertyInfo(	mTargetUnit,
                                                kAudioUnitProperty_CocoaUI,
                                                kAudioUnitScope_Global, 
                                                0,
                                                &dataSize,
                                                &isWritable );
    
    numberOfClasses = (dataSize - sizeof(CFURLRef)) / sizeof(CFStringRef);
    
    if ((result != noErr) || (numberOfClasses == 0)) {
        // If we get here, the audio unit does not have Cocoa UI.
        // A good app would display the Carbon UI in a separate window.
        // See "Integrating Carbon and Cocoa in your application" -- <http://developer.apple.com/documentation/Cocoa/Conceptual/CarbonCocoaDoc/index.html>
        [uiAUViewContainer setContentView:mEmptyView];
        return;
    } else {
        cocoaViewInfo = (AudioUnitCocoaViewInfo *)malloc(dataSize);
        NSAssert(AudioUnitGetProperty(	mTargetUnit,
                                        kAudioUnitProperty_CocoaUI,
                                        kAudioUnitScope_Global,
                                        0,
                                        cocoaViewInfo,
                                        &dataSize) == noErr,
                                        @"AudioUnitGetProperty(kAudioUnitProperty_CocoaUI)");
    }
    
    NSURL 	 *	viewBundlePath	= (NSURL *)cocoaViewInfo->mCocoaAUViewBundleLocation;
    NSBundle *	viewBundle  	= [NSBundle bundleWithPath:[viewBundlePath path]];
    NSString *	viewClassName	= (NSString *)cocoaViewInfo->mCocoaAUViewClass[0];		
    // Main Cocoa UI class name
    
    NSAssert(viewBundle != nil, @"Error loading AU view's bundle");
    
    Class viewClass = [viewBundle classNamed:viewClassName];
    // make sure 'viewClass' implements the AUCocoaUIBase protocol
    NSAssert(	[HostingWindowController plugInClassIsValid:viewClass],
                @"AU view's main class does not properly implement the AUCocoaUIBase protocol");
    
    id createdClass = [[viewClass alloc] init];	// instantiate principal class
    NSView *theView = [createdClass uiViewForAudioUnit: mTargetUnit
                                    withSize:[[uiAUViewContainer contentView] bounds].size];
    
    [uiAUViewContainer setContentView:theView];	// replace the current view with the new view
    [theView release];	// we release 'theView' because the uiAUViewContainer retains it, so
                        // when we go to replace 'theView' from the uiAUViewContainer,
                        // 'theView' will be deallocated.
    
    // release cocoaViewInfo's objects
    [viewBundlePath release];
    UInt32 i;
    for (i = 0; i < numberOfClasses; i++)
        CFRelease(cocoaViewInfo->mCocoaAUViewClass[i]);
    
    free (cocoaViewInfo);
}

#pragma mark ____AUGraph & file management____
- (void)addLinkToFiles:(NSArray *)inFiles {
    [mAudioFileList addObjectsFromArray:inFiles];
    [uiPlayStopButton setEnabled:[mAudioFileList count] > 0];
    [uiAudioFileTableView reloadData];
}

- (void)createGraph {
	NSAssert(NewAUGraph(&mGraph) == noErr, @"NewAUGraph()");
	
	ComponentDescription cd;
	cd.componentType = kAudioUnitType_Output;
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;
	cd.componentFlags = 0;
	cd.componentFlagsMask = 0;
    
	NSAssert(AUGraphNewNode(mGraph, &cd, 0, NULL, &mOutputNode) == noErr, @"AUGraphNewNode()");
	NSAssert(AUGraphOpen(mGraph) == noErr, @"AUGraphOpen()");
	NSAssert(AUGraphInitialize(mGraph) == noErr, @"AUGraphInitialize()");
    NSAssert(AUGraphGetNodeInfo(mGraph, mOutputNode, NULL, NULL, NULL, &mOutputUnit) == noErr, @"AUGraphGetNodeInfo()");
}

- (IBAction)iaPlayStopButtonPressed:(id)sender {
    if (sender == self) {
        // change button icon manually if we call this function internally
        [uiPlayStopButton setState:([uiPlayStopButton state] == NSOffState) ? NSOnState : NSOffState];
    }
    
    Boolean isRunning = FALSE;
	NSAssert(AUGraphIsRunning(mGraph, &isRunning) == noErr, @"AUGraphIsRunning()");
    if (isRunning) {
        // stop graph, release file, & return
        NSAssert(AUGraphStop(mGraph) == noErr, @"AUGraphStop()");
        if (mAFPID) {
            NSAssert(AFP_Disconnect(mAFPID) == noErr, @"AFP_Disconnect()");
            NSAssert(DisposeAudioFilePlayID(mAFPID) == noErr, @"DisposeAudioFilePlayID()");
        }
        [uiAudioFileNowPlayingName setStringValue:@""];
        [uiAUPopUpButton setEnabled:YES];
        return;
    }
    
    // otherwise load & play file
    FSRef destFSRef;
    int selectedRow = [uiAudioFileTableView selectedRow];
    if ( (selectedRow < 0) || ([mAudioFileList count] == 0) ) return;	// no file selected
    
    UInt8 *pathName = (UInt8 *)[(NSString*)[mAudioFileList objectAtIndex:selectedRow] cString];
    NSAssert(FSPathMakeRef(pathName, &destFSRef, NULL) == noErr, @"FSPathMakeRef()");
    // set filename in UI
    [uiAudioFileNowPlayingName setStringValue:[(NSString*)[mAudioFileList objectAtIndex:selectedRow] lastPathComponent]];
    [uiAUPopUpButton setEnabled:NO];
    
    NSAssert(NewAudioFilePlayID(&destFSRef, &mAFPID) == noErr, @"NewAudioFilePlayID()");
    NSAssert(AFP_SetNotifier(mAFPID, AudioFileNotificationHandler, self) == noErr, @"AFP_SetNotifier()");
    NSAssert(AFP_SetDestination(mAFPID, mTargetUnit, 0) == noErr, @"AFP_SetDestination()");
    NSAssert(AFP_Connect(mAFPID) == noErr, @"AFP_Connect()");
    NSAssert(AUGraphStart(mGraph) == noErr, @"AUGraphStart()");
}

#pragma mark ____PRESETS____
- (IBAction)iaRestorePresetButtonPressed:(id)sender {
    NSLog (@"not yet implemented");
}

- (IBAction)iaSavePresetButtonPressed:(id)sender {
    NSLog (@"not yet implemented");
}

#pragma mark ____TABLEVIEW IMPLEMENTATION____
- (int)numberOfRowsInTableView:(NSTableView *)inTableView {
    int count = [mAudioFileList count];
    return (count > 0) ? [mAudioFileList count] : 1;
}

- (id)tableView:(NSTableView *)inTableView objectValueForTableColumn:(NSTableColumn *)inTableColumn row:(int)inRow {
    int count = [mAudioFileList count];
    return (count > 0) ?	[(NSString *)[mAudioFileList objectAtIndex:inRow] lastPathComponent] :
                            @"< drag audio files here >";
}

@end
