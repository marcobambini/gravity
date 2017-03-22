//
//  BTFCodeEditor.m
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFCodeEditor.h"
#import "BTFRulerView.h"
#import "BTFThemeKeys.h"
#import "BTFTextView.h"

#define BTF_DEFAULT_RIGHT_PADDING		10.0f

@interface BTFCodeEditor() <NSTextDelegate, NSTextViewDelegate, BTFCodeEditorDelegate> {
    NSScrollView		*scrollView;
    BTFRulerView		*rulerView;
    BTFTextView			*textView;
    __weak NSWindow		*mainWindow;
	BOOL				collectChanges;
	
	// autocomplete
	NSMutableArray		*keys;
	
	// temp
	NSUInteger			tempLength;
	NSUInteger			tempIndex;
}
@end

@implementation BTFCodeEditor
@synthesize changed;

- (instancetype) initWithFrame:(NSRect)frame inView:(id)aView; {
    self = [super init];
    if (self) {
		collectChanges = NO;
		
        // setup scrollview
        scrollView = [[NSScrollView alloc] initWithFrame:frame];
        NSSize contentSize = [scrollView contentSize];
        [scrollView setBorderType:NSNoBorder];
        [scrollView setHasVerticalScroller:YES];
        [scrollView setHasHorizontalScroller:NO];
        [scrollView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
		
        // setup textview
        textView = [[BTFTextView alloc] initWithFrame:NSMakeRect(0, 0, contentSize.width, contentSize.height)];
		
		// setup rulerview
		rulerView = [[BTFRulerView alloc] initWithBTFTextView:textView];
		scrollView.verticalRulerView = rulerView;
		scrollView.hasVerticalRuler = true;
		scrollView.rulersVisible = true;
		
		// set textview settings
        [textView setMinSize:NSMakeSize(0.0, contentSize.height)];
        [textView setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
        [textView setVerticallyResizable:YES];
        [textView setHorizontallyResizable:NO];
        [textView setAutoresizingMask:NSViewWidthSizable];
        [[textView textContainer] setContainerSize:NSMakeSize(contentSize.width-(rulerView.ruleThickness + BTF_DEFAULT_RIGHT_PADDING), FLT_MAX)];
        [[textView textContainer] setWidthTracksTextView:YES];
		textView.BTFDelegate = self;
		textView.rulerView = rulerView;
		
        // connect scrollView to textView
        [scrollView setDocumentView:textView];
        
        // setup window connections (if any)
		if ([aView isKindOfClass:[NSView class]]) {
			[(NSView *)aView addSubview:scrollView];
		} else {
			NSWindow *aWindow = (NSWindow *)aView;
			mainWindow = aWindow;
			[aWindow setContentView:scrollView];
			[aWindow makeKeyAndOrderFront:nil];
			[aWindow makeFirstResponder:textView];
		}
		
		// setup autocompletion
		keys = [NSMutableArray array];
		
		// setup observers
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(frameDidChange:)
													 name:NSViewFrameDidChangeNotification object:textView];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(textDidChange:)
													 name:NSTextDidChangeNotification object:textView];
    }
    return self;
}

- (void)dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)setString:(NSString *)aString {
	collectChanges = NO;
	textView.string = (aString) ? aString : @"";
	[rulerView setNeedsDisplay:YES];
	collectChanges = YES;
}

- (NSString *)string {
	return textView.string;
}

- (void)setEditable:(BOOL)editable {
	[textView setEditable:editable];
}

- (void)setFocus {
	[textView.window makeFirstResponder:textView];
}

- (BOOL)editable {
	return textView.editable;
}

- (void) clearUndoManager:(NSUndoManager *)undoManager {
	[undoManager removeAllActionsWithTarget:textView.textStorage];
}

#pragma mark - Autocompletion -

- (void) resetAutocompleteEntities {
	[keys removeAllObjects];
}

- (void) addAutocompleteEntity:(NSString *)key type:(int)type {
	[keys addObject:key];
}

- (void) removeAutocompleteEntity:(NSString *)key {
	[keys removeObject:key];
}

#pragma mark - Temp -

- (void) tempString:(NSString *)s insertAtIndex:(NSUInteger)idx {
	[self tempStringRemove];
	if (!s) {[textView setNeedsDisplay:YES]; return;}
	
	// insert temp string into textview
	[textView setSelectedRange:NSMakeRange(idx,0)];
	[textView insertText:s];
	
	// set temp string coordinates
	tempIndex = idx;
	tempLength = s.length;
	[textView setNeedsDisplay:YES];
}

- (void) tempStringFinalize {
	tempLength = 0;
	[textView setNeedsDisplay:YES];
}

- (void) tempStringRemove {
	if (tempLength == 0) return;
	NSRange range = NSMakeRange(tempIndex, tempLength);
	NSString *string = [textView textStorage].string;
	
	// sanity check range before apply the change
	if (range.location != NSNotFound && range.location + range.length <= string.length) {
		[[textView textStorage] deleteCharactersInRange:range];
	}
	tempLength = 0;
}

- (NSUInteger) mousePointToIndex:(NSPoint)p {
	// p is in screen coordinates
	NSRect rect = [textView.window convertRectFromScreen:NSMakeRect(p.x, p.y, 0, 0)];
	NSPoint point = [textView convertPoint:rect.origin fromView:nil];
	
	// sanity check
	if ((point.x < 0.0f) || (point.y < 0.0) ||
		(floor(point.x) == 0) || (floor(point.y) == 0)) return NSUIntegerMax;
	
	// compute insertion index
	NSUInteger insertionIndex = [textView characterIndexForInsertionAtPoint:point];
	if (tempLength) {
		if (insertionIndex >= tempIndex && insertionIndex <= tempIndex+tempLength) insertionIndex = tempIndex;
		else if (insertionIndex > tempIndex+tempLength) insertionIndex -= tempLength;
	}
	
	return insertionIndex;
}

#pragma mark - Observers -

- (void) frameDidChange:(NSNotification *)notification {
	NSSize contentSize = [scrollView contentSize];
	[[textView textContainer] setContainerSize:NSMakeSize(contentSize.width-(rulerView.ruleThickness + BTF_DEFAULT_RIGHT_PADDING), FLT_MAX)];
	[rulerView setNeedsDisplay:YES];
}

- (void) textDidChange:(NSNotification *)notification {
	if (collectChanges) changed = YES;
	[rulerView setNeedsDisplay:YES];
}

#pragma mark - Others -

- (void) applyTheme:(NSDictionary *)theme {
	// process keys that need to be always evaluated
	if (theme[BTFKEY_RULER_BACKGROUND_COLOR])
		rulerView.backgroundColor = theme[BTFKEY_RULER_BACKGROUND_COLOR];
	if (theme[BTFKEY_RULER_TEXT_COLOR])
		rulerView.textColor = theme[BTFKEY_RULER_TEXT_COLOR];
	if (theme[BTFKEY_RULER_BORDER_COLOR])
		rulerView.borderColor = theme[BTFKEY_RULER_BORDER_COLOR];
	else
		rulerView.borderColor = [NSColor clearColor];
	if (theme[BTFKEY_RULER_BORDER_WIDTH])
		rulerView.borderWidth = ((NSNumber*)theme[BTFKEY_RULER_BORDER_WIDTH]).floatValue;
	else
		rulerView.borderWidth = 0;
	
	[rulerView setNeedsDisplay:YES];
	textView.theme = theme;
}

- (CGFloat)ruleThickness {
	return rulerView.ruleThickness;
}

#pragma mark - BTFCodeEditorDelegate -

- (NSArray *)textView:(NSTextView *)aTextView completions:(NSArray *)words forPartialWordRange:(NSRange)charRange
			 location:(NSUInteger)location indexOfSelectedItem:(NSInteger *)index {
	
	NSMutableArray *matches = [NSMutableArray array];
	NSString *text = [textView string];
	NSString *target = [text substringWithRange:charRange];
	
//	NSLog(@"%@", target);
//	NSLog(@"%@", words);
	
//	// do not autocomplete if in the middle of the string
//	if (location < charRange.location + charRange.length) return matches;
//	
//	// autocomplete only if next character is not alpha
//	if (text.length > charRange.location + charRange.length) {
//		NSRange nextRange = NSMakeRange(charRange.location+1, 1);
//		unichar nextChar = [[text substringWithRange:nextRange] characterAtIndex:0];
//		bool isAlpha = ((nextChar >= 97 && nextChar <=122) || (nextChar >= 65 && nextChar <=90));
//		NSLog(@"%d %d (%@)", isAlpha, nextChar, [text substringWithRange:nextRange]);
//		if (!isAlpha) return matches;
//	}
	
	if (words.count == 0) {
		// scan keywords ONLY if there are no subtargets
		for (NSString *string in keys) {
			// do not autocomplete equal strings
			if ([target isEqualToString:string]) continue;
			
			// search matches
			if ([string rangeOfString:target options:NSAnchoredSearch | NSCaseInsensitiveSearch
								range:NSMakeRange(0, [string length])].location != NSNotFound) {
				[matches addObject:string];
			}
		}
	}
	[matches sortUsingSelector:@selector(compare:)];
	return matches;
}

- (NSImage *)textView:(NSTextView *)textView imageForCompletion:(NSString *)word {
//	NSImage *image = self.imageDict[word];
//	if (image) {
//		return image;
//	}
//	return self.imageDict[@"Unknown"];
	return nil;
}

- (void)textDidEndEditing:(NSNotification *)notification {
	if (!self.delegate) return;
	if ([self.delegate respondsToSelector:@selector(textDidEndEditing:)]) {
		[self.delegate textDidEndEditing:self];
	}
}

@end
