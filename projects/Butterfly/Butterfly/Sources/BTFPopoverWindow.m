//
//  BTFPopoverWindow.m
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFPopoverWindow.h"

@interface BTFPopoverWindow() {
	NSView	*popoverContentView;
}
@end

@implementation BTFPopoverWindow

- (id) initWithContentRect:(NSRect)contentRect backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation {
	if((self = [super initWithContentRect:contentRect styleMask:NSBorderlessWindowMask backing:bufferingType defer:deferCreation])) {
		[self setBackgroundColor:[NSColor clearColor]];
		[self setMovableByWindowBackground:NO];
		[self setExcludedFromWindowsMenu:YES];
		[self setAlphaValue:1];
		[self setOpaque:NO];
		[self setHasShadow:YES];
	}
	return self;
}

- (BOOL) canBecomeKeyWindow {
	return NO;
}

- (BOOL) canBecomeMainWindow {
	return NO;
}

@end
