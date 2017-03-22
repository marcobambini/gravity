//
//  BTFTextView.h
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "BTFDelegate.h"
#import "BTFRulerView.h"

@interface BTFTextView : NSTextView
@property (nonatomic, weak) id <BTFCodeEditorDelegate>	BTFDelegate;
@property (nonatomic, weak) BTFRulerView				*rulerView;
@property (nonatomic)		NSDictionary				*theme;
@end
