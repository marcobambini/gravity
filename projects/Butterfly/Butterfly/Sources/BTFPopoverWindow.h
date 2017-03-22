//
//  BTFPopoverWindow.h
//  Butterfly
//
//  Created by Marco Bambini on 21/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface BTFPopoverWindow : NSWindow
- (id) initWithContentRect:(NSRect)contentRect backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation;
@end
