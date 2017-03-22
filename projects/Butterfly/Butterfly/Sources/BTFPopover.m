//
//  BTFPopover.m
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFPopover.h"
#import "BTFPopoverFrame.h"

@interface BTFPopover() {
	NSViewController	*contentViewController;
	BTFPopoverWindow	*popoverWindow;
	BTFPopoverFrame		*popoverFrame;
}
@end

@implementation BTFPopover

- (id) init {
	return [self initWithContentViewController:nil];
}

- (id) initWithContentView:(NSView *)contentView {
	NSViewController *aContentViewController = [[NSViewController alloc] init];
	
	popoverFrame = [[BTFPopoverFrame alloc] initWithFrame:NSZeroRect];
	[popoverFrame addSubview:contentView];
	
	[aContentViewController setView:popoverFrame];
	return [self initWithContentViewController:aContentViewController];
}

- (id) initWithContentViewController:(NSViewController *)aContentViewController {
	if((self = [super init])) {
		contentViewController = aContentViewController;
		
		NSView *contentView = [contentViewController view];
		popoverWindow = [[BTFPopoverWindow alloc] initWithContentRect:[contentView frame] backing:NSBackingStoreBuffered defer:YES];
		[popoverWindow setContentView:contentView];
		[popoverWindow setMinSize:[contentView frame].size];
		
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResignKey:)
													 name:NSWindowDidResignKeyNotification object:popoverWindow];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationDidResignActive:)
													 name:NSApplicationDidResignActiveNotification object:nil];
	}
	return self;
}

- (void) displayPopoverInWindow:(NSWindow *)window atPoint:(NSPoint)point {
	BOOL wasVisible = [popoverWindow isVisible];
	[popoverWindow setFrameOrigin:point];
	if (wasVisible) return;
	
	[window addChildWindow:popoverWindow ordered:NSWindowAbove];
	[popoverWindow makeKeyAndOrderFront:nil];
}

- (void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (NSWindow *) window {
	return popoverWindow;
}

- (BOOL) isVisible {
	return [popoverWindow isVisible];
}

- (void) closePopover:(id)sender {
	if(![popoverWindow isVisible]) return;
	
	NSWindow *parentWindow = [popoverWindow parentWindow];
	[parentWindow removeChildWindow:popoverWindow];
	[popoverWindow orderOut:sender];
}
@end

@implementation BTFPopover (NSWindowDelegateMethods)

- (void) windowDidResignKey:(NSNotification *)notification {
	//NSLog(@"windowDidResignKey");
	//if(self.closesWhenPopoverResignsKey)
		//[self closePopover:notification];
}

- (void) applicationDidResignActive:(NSNotification *)notification{
	//NSLog(@"applicationDidResignActive");
		//[self closePopover:notification];
}

@end
