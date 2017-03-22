//
//  BTFPopover.h
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

//
// Although the majority of the code was written by me, the inspiration and some code comes from:
// NCRAutocompleteTextView by Daniel Weber on 9/28/14 - Copyright (c) 2014 Null Creature.
// SFBPopover - Copyright (C) 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
//

#import <Cocoa/Cocoa.h>
#import	"BTFPopoverWindow.h"

@interface BTFPopover : NSResponder

- (id) initWithContentView:(NSView *)contentView;
- (void) displayPopoverInWindow:(NSWindow *)window atPoint:(NSPoint)point;
- (void) closePopover:(id)sender;
- (BTFPopoverWindow *) window;
- (BOOL) isVisible;
@end
