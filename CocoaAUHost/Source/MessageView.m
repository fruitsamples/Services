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
//
//  MessageView.m
//  AudioMIDISetup
//
//  Created by Luke Bellandi on Wed Mar 26 2003.
//  Copyright (c) 2003 Apple Computer. All rights reserved.
//

#import "MessageView.h"

#define MESSAGE_TEXT_SIZE		24.0f

@implementation MessageView

- (id)initWithFrame:(NSRect)inRect text:(NSString *)inText {
    return [self initWithFrame:inRect text:inText proportionalTextSize:1.0f];
}

- (id)initWithFrame:(NSRect)inRect text:(NSString *)inText proportionalTextSize:(float)inSize {
    self = [super initWithFrame:inRect];
    if (self) {
        NSDictionary *stringAttributes =	[NSDictionary	dictionaryWithObjects:	[NSArray arrayWithObjects:[NSFont systemFontOfSize:(MESSAGE_TEXT_SIZE * inSize)],
                                                                                    [NSColor colorWithCalibratedWhite:0.000f alpha:0.500f], nil] 
                                            forKeys:[NSArray arrayWithObjects:NSFontAttributeName, NSForegroundColorAttributeName, nil]];
        mPanelLoadingString = [[NSAttributedString alloc]	initWithString:inText
                                                            attributes:stringAttributes];
        mPLStringSize = [mPanelLoadingString size];
        
        // make view size-to-fit
        [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    }
    return self;
}

- (void)dealloc {
    [mPanelLoadingString release];
}

- (void)drawRect:(NSRect)inRect {
    NSRect frameRect = [self frame];
    [mPanelLoadingString drawAtPoint:NSMakePoint ((frameRect.size.width - mPLStringSize.width) * 0.5f, (frameRect.size.height - mPLStringSize.height) * 0.5f)];
}

- (BOOL)isFlipped	{	return YES;	}

@end

/*
 */
