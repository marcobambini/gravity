//
//  BTFRulerView.h
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@class BTFTextView;
@interface BTFRulerView : NSRulerView

@property (nonatomic) NSColor *backgroundColor;
@property (nonatomic) NSColor *textColor;
@property (nonatomic) NSColor *borderColor;
@property (nonatomic) CGFloat borderWidth;

- (instancetype)initWithBTFTextView:(BTFTextView *)textView;

@end
