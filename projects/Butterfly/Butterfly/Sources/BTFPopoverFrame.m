//
//  BTFPopoverFrame.m
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFPopoverFrame.h"

@implementation BTFPopoverFrame

- (id) initWithFrame:(NSRect)frame {
	if ((self = [super initWithFrame:frame])) {
		[self setWantsLayer:YES];
		self.layer.backgroundColor = [NSColor colorWithCalibratedWhite:(CGFloat)1.0 alpha:(CGFloat)0.9].CGColor;
		self.layer.borderColor = [NSColor darkGrayColor].CGColor;
		self.layer.borderWidth = 0.5f;
		self.layer.cornerRadius = 5;
		self.layer.masksToBounds = YES;
	}
	return self;
}

@end
