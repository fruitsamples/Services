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
	main.cpp

=============================================================================*/

/*========================================================

    DiskWriterGraph: tool to write the contents of a MIDI File to AIFC files (1 file per output channel)
    
========================================================*/
#include <iostream>
#include <unistd.h>

#include "ProducerAUGraph.h"

int main (int argc, const char * argv[]) {
    
    // requires a MIDI File as argument
    if (argc != 2) {
        printf ("usage:     DiskWriterGraph [MIDIfilename]\n\n");
        printf ("function:  Renders digital audio data from a MIDI File.\n");
        printf ("           Accomplishes this by using an AUGraph.\n");
        printf ("           Creates AIFF (.aif) files: one file for each output channel.\n\n");
        fflush (stdout);
        return 1;
    }
    
    char	baseDir[1024];
    char	baseFilename[1024];
    SInt32	i, len;
    
    strcpy (baseDir, argv[1]);
        
    len = strlen (argv[1]) - 1;
    // get base directory
    for (i = len; i >= 0; i--) {
        if (baseDir[i] == '/') {
                strcpy (baseFilename, &(baseDir[i + 1]));
                baseDir[i + 1] = 0;
            break;
        }
    }
    
    // special case for local-file
    if (i <= 0) {
        strcpy (baseFilename, baseDir);
        strcpy (baseDir, "./");
    }
    
    // get base filename
    len = strlen (baseFilename);
    for (i = 0; i < len; i++) {
        if (baseFilename[i] == '.') {
            baseFilename[i] = 0;
            break;
        }
    }
    
    // heart of the application
    ProducerAUGraph *PAUG = new ProducerAUGraph (argv[1], baseDir, baseFilename);
    Float32 monitorValue = 0.10f;
    
    printf ("Writing audio to disk:\n");
    fflush (stdout);
    
    // wait until graph is done (& monitor its progress)
    while (PAUG->IsRunning()) {
        usleep ( 250 * 1000 );
        if ( PAUG->GetPercentComplete() > monitorValue ) {
            fprintf (stdout, "%d%% ", (int)(monitorValue * 100));
            fflush (stdout);
            monitorValue += 0.10f;
        }
    }
    delete PAUG;
    
    printf ("completed.\n"); fflush (stdout);
    
    return 0;
    
}
