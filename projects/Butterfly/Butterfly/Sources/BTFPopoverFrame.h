//
//  BTFPopoverFrame.h
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface BTFPopoverFrame : NSView

@property (nonatomic, copy)	  NSColor *borderColor;
@property (nonatomic, assign) CGFloat borderWidth;
@property (nonatomic, assign) CGFloat cornerRadius;
@property (nonatomic, copy)	  NSColor *backgroundColor;

@end
