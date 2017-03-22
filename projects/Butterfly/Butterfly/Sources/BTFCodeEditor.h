//
//  BTFCodeEditor.h
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "BTFDelegate.h"

@interface BTFCodeEditor : NSObject

@property (nonatomic, weak) id<BTFCodeEditorDelegate>	delegate;
@property (nonatomic)		NSString					*string;
@property (nonatomic)		BOOL						editable;
@property (readonly)		CGFloat						ruleThickness;
@property (nonatomic)		BOOL						changed;

- (instancetype) initWithFrame:(NSRect)frame inView:(id)aView;
- (void) applyTheme:(NSDictionary *)theme;
- (void) setFocus;

- (void) resetAutocompleteEntities;
- (void) addAutocompleteEntity:(NSString *)key type:(int)type;
- (void) removeAutocompleteEntity:(NSString *)key;

- (NSUInteger) mousePointToIndex:(NSPoint)p;
- (void) tempString:(NSString *)s insertAtIndex:(NSUInteger)idx;
- (void) tempStringFinalize;
- (void) clearUndoManager:(NSUndoManager *)undoManager;

@end
