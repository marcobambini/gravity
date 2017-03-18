//
//  BTFDelegate.h
//  Butterfly
//
//  Created by Marco Bambini on 11/09/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

@protocol BTFCodeEditorDelegate <NSObject>
@optional
- (void) mouseMovedAtRange:(NSRange)range;
- (void) textDidEndEditing:(id)sender;
- (NSArray *)textView:(NSTextView *)textView completions:(NSArray *)words forPartialWordRange:(NSRange)charRange
			 location:(NSUInteger)location indexOfSelectedItem:(NSInteger *)index;
//- (NSImage *)textView:(NSTextView *)textView imageForCompletion:(NSString *)word;
@end
