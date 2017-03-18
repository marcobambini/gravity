//
//  BTFTextView.m
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFThemeKeys.h"
#import "BTFTextView.h"
#import "BTFPopover.h"
#import "gravity_parser.h"

#define BTF_DEFAULT_INSET_VALUE		10.0f
#define BTF_MAX_RESULTS				10
#define BTF_INTERCELL_SPACING		NSMakeSize(20.0f, 3.0f)
#define BTF_DEFAULT_FONT			[NSFont fontWithName:@"Menlo" size:12.0f]
#define BTF_DEFAULT_BOLDFONT		[NSFont fontWithName:@"Menlo-Bold" size:12.0f]

#define BTF_POPOVER_DISTANCE		5.0f
#define BTF_POPOVER_WIDTH			375.0f
#define BTF_POPOVER_PADDING			0.0f
#define BTF_POPOVER_FONT			BTF_DEFAULT_FONT
#define BTF_POPOVER_BOLDFOND		BTF_DEFAULT_BOLDFONT
#define BTF_POPOVER_TEXTCOLOR		[NSColor blackColor]

#define BTF_HIGHLIGHT_STROKE_COLOR	[NSColor selectedMenuItemColor]
#define BTF_HIGHLIGHT_FILL_COLOR	[NSColor selectedMenuItemColor]
#define BTF_HIGHLIGHT_RADIUS		0.0f

#pragma mark -

@interface BTFAutocompleteTableRowView : NSTableRowView
@end
@implementation BTFAutocompleteTableRowView
- (void)drawSelectionInRect:(NSRect)dirtyRect {
	if (self.selectionHighlightStyle != NSTableViewSelectionHighlightStyleNone) {
		NSRect selectionRect = NSInsetRect(self.bounds, 0.5, 0.5);
		[BTF_HIGHLIGHT_STROKE_COLOR setStroke];
		[BTF_HIGHLIGHT_FILL_COLOR setFill];
		NSBezierPath *selectionPath = [NSBezierPath bezierPathWithRoundedRect:selectionRect xRadius:BTF_HIGHLIGHT_RADIUS yRadius:BTF_HIGHLIGHT_RADIUS];
		[selectionPath fill];
		[selectionPath stroke];
	}
}
- (NSBackgroundStyle)interiorBackgroundStyle {
	if (self.isSelected) {
		return NSBackgroundStyleDark;
	} else {
		return NSBackgroundStyleLight;
	}
}
@end

#pragma mark -

@interface BTFTextView() <NSTableViewDataSource, NSTableViewDelegate, NSTextViewDelegate, NSTextDelegate> {
	BTFPopover			*autocompletePopover;
	NSTableView			*autocompleteTableView;
	BOOL				autocompleteSupported;
	NSCharacterSet		*characterSet;
	
	NSArray				*matches;
	NSString			*substring;
	NSInteger			lastPosition;
	NSDictionary		*theme;
	
	NSFont				*fontRegular;
	NSFont				*fontBold;
	
	gravity_delegate_t	compiler_delegate;
}
@end

static void	errorCallback (error_type_t error_type, const char *description, error_desc_t error_desc, void *xdata) {
//	BTFTextView *textView = (__bridge BTFTextView *)xdata;
//	
//	const char *type = "N/A";
//	switch (error_type) {
//		case GRAVITY_ERROR_NONE: type = "NONE"; break;
//		case GRAVITY_ERROR_SYNTAX: type = "SYNTAX"; break;
//		case GRAVITY_ERROR_SEMANTIC: type = "SEMANTIC"; break;
//		case GRAVITY_ERROR_IO: type = "I/O"; break;
//		case GRAVITY_ERROR_RUNTIME: type = "RUNTIME"; break;
//		case GRAVITY_WARNING: type = "WARNING"; break;
//	}
//	
//	if (error_type == GRAVITY_ERROR_RUNTIME) printf("RUNTIME ERROR: ");
//	else printf("%s ERROR (code %d) on %s (%d,%d)\n", type, error_desc.error_code, error_desc.filename, error_desc.lineno, error_desc.colno);
//	printf("%s\n", description);
}

static void parserCallback (void *xtoken, void *xdata) {
	BTFTextView		*textView = (__bridge BTFTextView *) xdata;
	NSDictionary	*theme = textView.theme;
	gtoken_s		*token = (gtoken_s *)xtoken;
	
	[textView.textStorage beginEditing];
	NSRange range = NSMakeRange(token->position, token->length);
	@try {
		switch (token->type) {
			case TOK_COMMENT:
				[textView setTextColor:theme[BTFKEY_COMMENTS_COLOR] range:range];
				break;
				
			case TOK_IDENTIFIER:
				[textView setTextColor:theme[BTFKEY_PLAIN_COLOR] range:range];
				break;
			
			case TOK_MACRO:
			case TOK_KEY_IMPORT:
				[textView setTextColor:theme[BTFKEY_MACROS_COLOR] range:range];
				break;
			
			case TOK_KEY_ENUM:
			case TOK_KEY_MODULE:
			case TOK_KEY_VAR:
			case TOK_KEY_CONST:
			case TOK_KEY_CLASS:
			case TOK_KEY_FUNC:
			case TOK_KEY_SUPER:
			case TOK_KEY_DEFAULT:
			case TOK_KEY_TRUE:
			case TOK_KEY_FALSE:
			case TOK_KEY_IF:
			case TOK_KEY_ELSE:
			case TOK_KEY_SWITCH:
			case TOK_KEY_BREAK:
			case TOK_KEY_CONTINUE:
			case TOK_KEY_RETURN:
			case TOK_KEY_WHILE:
			case TOK_KEY_REPEAT:
			case TOK_KEY_FOR:
			case TOK_KEY_IN:
			case TOK_KEY_PRIVATE:
			case TOK_KEY_INTERNAL:
			case TOK_KEY_PUBLIC:
			case TOK_KEY_STATIC:
			case TOK_KEY_EXTERN:
			case TOK_KEY_CASE:
			case TOK_KEY_EVENT:
			case TOK_KEY_FILE:
			case TOK_KEY_NULL:
			case TOK_KEY_UNDEFINED:
			case TOK_KEY_CURRARGS:
				[textView setTextColor:theme[BTFKEY_KEYWORDS_COLOR] range:range];
				break;
				
			case TOK_STRING:
				// in case of literal string, returned token is the string itself and
				// not the string surronded by the quotes, so let's fix it!
				range.location-=1; range.length+=2;
				[textView setTextColor:theme[BTFKEY_STRINGS_COLOR] range:range];
				break;
				
			case TOK_NUMBER:
				[textView setTextColor:theme[BTFKEY_NUMBERS_COLOR] range:range];
				break;
				
			case TOK_SPECIAL:
				[textView setTextColor:theme[BTFKEY_SPECIAL_COLOR] range:range];
				break;
				
			case TOK_ERROR: {
				[textView setTextColor:theme[BTFKEY_PLAIN_COLOR] range:range];
				}
				break;
				
			default:
				[textView setTextColor:theme[BTFKEY_PLAIN_COLOR] range:range];
				break;
		}		
	}
	@catch (NSException *exception) {
		NSLog(@"%@", exception.reason);
	}
	@finally {
		//NSLog(@"(%d, %d)\t%s\t%@", token->lineno, token->colno, token_name(token->type), NSStringFromRange(range));
		//NSLog(@"%@", [textView.string substringWithRange:range]);
		[textView.textStorage endEditing];
	}
}

static BOOL is_word_boundary (unichar c) {
	if (c == '_') return NO;
	if (isspace(c)) return YES;
	if (iscntrl(c)) return YES;
	if (ispunct(c)) return YES;
	return NO;
}

@implementation BTFTextView

- (instancetype)initWithFrame:(NSRect)frameRect textContainer:(NSTextContainer *)aTextContainer {
	self = [super initWithFrame:frameRect textContainer:aTextContainer];
	if (self) {
		// default TextView settings
		[self setTextContainerInset:NSMakeSize(BTF_DEFAULT_INSET_VALUE, BTF_DEFAULT_INSET_VALUE)];
		[self setFont:BTF_DEFAULT_FONT];
		[self setRichText:YES];
		
		[self setAutomaticTextReplacementEnabled:NO];
		[self setAutomaticSpellingCorrectionEnabled:NO];
		[self setAutomaticDataDetectionEnabled:NO];
		[self setAutomaticDashSubstitutionEnabled:NO];
		[self setEnabledTextCheckingTypes:NO];
		
		[self setDelegate:self];
		[self setUpAutocompletion];
		characterSet = [[NSCharacterSet alphanumericCharacterSet] invertedSet];
		
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(textDidEndEditing:)
													 name:NSControlTextDidEndEditingNotification object:nil];
		
		self.allowsUndo = YES;
		
		compiler_delegate.xdata = (__bridge void *)(self);
		compiler_delegate.parser_callback = parserCallback;
		compiler_delegate.error_callback = errorCallback;
	}
	return self;
}

- (void) dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self name:NSControlTextDidEndEditingNotification object:nil];
}

- (void) setTheme:(NSDictionary *)aTheme {
	// process keys that need to be always evaluated (it should work for NSString {v1,v2}
	if (aTheme[BTFKEY_INSET_VALUE]) [self setTextContainerInset:[aTheme[BTFKEY_INSET_VALUE] sizeValue]];
	if (aTheme[BTFKEY_BACKGROUND_COLOR]) [self setBackgroundColor:aTheme[BTFKEY_BACKGROUND_COLOR]];
	if (aTheme[BTFKEY_SELECTION_COLOR]) [self setSelectedTextAttributes:@{NSBackgroundColorAttributeName:aTheme[BTFKEY_SELECTION_COLOR]}];
	if (aTheme[BTFKEY_CURSOR_COLOR]) [self setInsertionPointColor:aTheme[BTFKEY_CURSOR_COLOR]];
	if (aTheme[BTFKEY_FONT]) [self setFont:aTheme[BTFKEY_FONT]];
	if (aTheme[BTFKEY_PLAIN_COLOR]) [self setTextColor:aTheme[BTFKEY_PLAIN_COLOR]];
	theme = aTheme;
}

- (NSDictionary *)theme {
	return theme;
}

- (void) setUpAutocompletion {
	// Make a table view with 1 column and enclosing scroll view. It doesn't
	// matter what the frames are here because they are set when the popover
	// is displayed
	NSTableColumn *column1 = [[NSTableColumn alloc] initWithIdentifier:@"text"];
	[column1 setEditable:NO];
	[column1 setWidth:BTF_POPOVER_WIDTH - 2 * BTF_POPOVER_PADDING];
	
	NSTableView *tableView = [[NSTableView alloc] initWithFrame:NSZeroRect];
	[tableView setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleRegular];
	[tableView setBackgroundColor:[NSColor clearColor]];
	[tableView setRowSizeStyle:NSTableViewRowSizeStyleSmall];
	[tableView setIntercellSpacing:BTF_INTERCELL_SPACING];
	[tableView setHeaderView:nil];
	[tableView setRefusesFirstResponder:YES];
	[tableView setTarget:self];
	[tableView setDoubleAction:@selector(insert:)];
	[tableView addTableColumn:column1];
	[tableView setDelegate:self];
	[tableView setDataSource:self];
	autocompleteTableView = tableView;
	
	NSScrollView *tableScrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
	[tableScrollView setDrawsBackground:NO];
	[tableScrollView setDocumentView:tableView];
	[tableScrollView setHasVerticalScroller:YES];
		
	// Setup custom popover
	autocompletePopover = [[BTFPopover alloc] initWithContentView:tableScrollView];
	
	// Finish setup
	matches = @[];
	lastPosition = -1;
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(didChangeSelection:) name:@"NSTextViewDidChangeSelectionNotification" object:nil];
}

-(void)setString:(NSString *)aString {
	[super setString:aString];
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wnonnull"
	[self textDidChange:nil];
	#pragma clang diagnostic pop
}

- (void)textDidChange:(NSNotification*)notification {
	NSString *code = [self string];
	NSRange range = NSMakeRange(0, [code length]);
	
	[self setTextColor:theme[BTFKEY_PLAIN_COLOR] range:range];
	// DO NOT USE [code lenght] here, otherwise UTF-8 characters are not counted!
	gravity_parser_t *parser = gravity_parser_create([code UTF8String], [code lengthOfBytesUsingEncoding:NSUTF8StringEncoding], 0, true);
	gravity_parser_run(parser, &compiler_delegate);
	gravity_parser_free(parser);
}

- (void)textDidEndEditing:(NSNotification *)notification {
	if (!self.BTFDelegate) return;
	if ([self.BTFDelegate respondsToSelector:@selector(textDidEndEditing:)]) {
		[self.BTFDelegate textDidEndEditing:self];
	}
}

- (void)setBTFDelegate:(id<BTFCodeEditorDelegate>)aBTFDelegate {
	_BTFDelegate = aBTFDelegate;
	autocompleteSupported = [self.BTFDelegate respondsToSelector:@selector(textView:completions:forPartialWordRange:location:indexOfSelectedItem:)];
}

#pragma mark - Autocompletion -

- (void)keyDown:(NSEvent *)theEvent {
	NSInteger row = autocompleteTableView.selectedRow;
	BOOL shouldComplete = YES;
	
	switch (theEvent.keyCode) {
		case 51:	// Delete key
			[autocompletePopover closePopover:self];
			break;
		case 53:	// Esc key
			if (autocompletePopover.isVisible)
				[autocompletePopover closePopover:self];
			return; // Skip default behavior
		case 125:	// Down arrow
			if (autocompletePopover.isVisible) {
				[autocompleteTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:row+1] byExtendingSelection:NO];
				[autocompleteTableView scrollRowToVisible:autocompleteTableView.selectedRow];
				return; // Skip default behavior
			}
			break;
		case 126:	// Up arrow
			if (autocompletePopover.isVisible) {
				[autocompleteTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:row-1] byExtendingSelection:NO];
				[autocompleteTableView scrollRowToVisible:autocompleteTableView.selectedRow];
				return; // Skip default behavior
			}
			break;
		case 36:	// Return key
		case 48:	// Tab key
			if (autocompletePopover.isVisible) {
				[self insert:self];
				return; // Skip default behavior
			}
		case 124:	// Right arrow
		case 123:	// Left arrow
		case 49:	// Space key
			if (autocompletePopover.isVisible) {
				[autocompletePopover closePopover:self];
			}
			break;
	}

	[super keyDown:theEvent];
	if (shouldComplete) {
		[self complete:self];
	}
}

- (void)didChangeSelection:(NSNotification *)notification {
	if ((self.selectedRange.location - lastPosition) > 1) {
		// if selection moves by more than just one character, hide autocomplete
		[autocompletePopover closePopover:self];
	}
}

- (void)paste:(id)sender {
	[self pasteAsPlainText:sender];
	[self complete:nil];
}

- (void)copy:(id)sender {
	NSRange range = self.selectedRange;
	NSString *text = (range.location != NSNotFound) ? [self.string substringWithRange:self.selectedRange] : self.string;
	NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
	[pasteBoard declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
	[pasteBoard setString:text forType:NSStringPboardType];
}

- (void)insert:(id)sender {
	if (autocompleteTableView.selectedRow >= 0 && autocompleteTableView.selectedRow < matches.count) {
		NSString *string = [matches objectAtIndex:autocompleteTableView.selectedRow];
		NSInteger beginningOfWord = self.selectedRange.location - substring.length;
		NSRange range = NSMakeRange(beginningOfWord, substring.length);
		if ([self shouldChangeTextInRange:range replacementString:string]) {
			[self replaceCharactersInRange:range withString:string];
			[self didChangeText];
		}
	}
	[autocompletePopover closePopover:self];
}

- (void)complete:(id)sender {
	NSInteger startOfWord = self.selectedRange.location;
	
	// extract member access (if any) Navigation1.Window1.Label1.text
	NSMutableArray *completions = [NSMutableArray array];
	NSInteger dotIndex = 0;
	for (NSInteger i = startOfWord - 1; i >= 0; --i) {
		unichar c = [self.string characterAtIndex:i];
		if (c == '.') {
			// check if it is a multi member case
			if (dotIndex != 0) {
				NSString *member = [self.string substringWithRange:NSMakeRange(i+1, dotIndex-(i+1))];
				if ((member == nil) || member.length == 0) {[completions removeAllObjects]; break;}
				[completions addObject:member];
			}
			dotIndex = i;
			continue;
		}
		
		// member access lookup break condition
		BOOL isBoundary = is_word_boundary(c);
		if (isBoundary && dotIndex == 0) break;
		
		// member case
		if (((i == 0) || isBoundary) && dotIndex != 0) {
			NSString *member = [self.string substringWithRange:NSMakeRange(i, dotIndex-i)];
			if ((member == nil) || member.length == 0) {[completions removeAllObjects]; break;}
			[completions addObject:member];
			dotIndex = 0;
		}
	}
	
	// extract current word
	for (NSInteger i = startOfWord - 1; i >= 0; --i) {
		if ([characterSet characterIsMember:[self.string characterAtIndex:i]]) break;
		--startOfWord;
	}
	
	// extract current lenght
	NSInteger lengthOfWord = 0;
	for (NSInteger i = startOfWord; i < self.string.length; ++i) {
		if ([characterSet characterIsMember:[self.string characterAtIndex:i]]) break;
		++lengthOfWord;
	}
	
	substring = [self.string substringWithRange:NSMakeRange(startOfWord, lengthOfWord)];
	NSRange substringRange = NSMakeRange(startOfWord, self.selectedRange.location - startOfWord);
	
	// NSLog(@"%@ %ld %ld %lu", substring, (long)startOfWord, (long)lengthOfWord, (unsigned long)self.selectedRange.location);
	
	// This happens when we just started a new word or if we have already typed the entire word
	if ((substringRange.length == 0 || lengthOfWord == 0) && (completions.count == 0)) {
		[autocompletePopover closePopover:self];
		return;
	}
	
	NSInteger index = 0;
	if (autocompleteSupported) {
		NSArray *r = [[completions reverseObjectEnumerator] allObjects];
		matches = [self.BTFDelegate textView:self completions:r forPartialWordRange:substringRange
									location:self.selectedRange.location indexOfSelectedItem:&index];
	} else matches = @[];
	
	if (matches.count > 0) {
		lastPosition = self.selectedRange.location;
		[autocompleteTableView reloadData];
		
		[autocompleteTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
		[autocompleteTableView scrollRowToVisible:index];
		
		// Make the frame for the popover. We want it to shrink with a small number
		// of items to autocomplete but never grow above a certain limit when there
		// are a lot of items. The limit is set by MAX_RESULTS.
		NSInteger numberOfRows = MIN(autocompleteTableView.numberOfRows, BTF_MAX_RESULTS);
		CGFloat height = (autocompleteTableView.rowHeight + autocompleteTableView.intercellSpacing.height) * numberOfRows + 2 * BTF_POPOVER_PADDING;
		NSRect frame = NSMakeRect(0, 0, BTF_POPOVER_WIDTH, height);
		[autocompleteTableView.enclosingScrollView setFrame:NSInsetRect(frame, BTF_POPOVER_PADDING, BTF_POPOVER_PADDING)];
		[[autocompletePopover window] setContentSize:NSMakeSize(NSWidth(frame), NSHeight(frame))];
		
		// Find best coord to display the popover
		NSRect rect = [self firstRectForCharacterRange:substringRange actualRange:NULL];
		rect.origin.y -= (height + BTF_POPOVER_DISTANCE);
		
		[autocompletePopover displayPopoverInWindow:self.window	atPoint:rect.origin];
	} else {
		[autocompletePopover closePopover:self];
	}
}

#pragma mark - NSTableView Data Source -

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
	return matches.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row {
	NSTableCellView *cellView = [tableView makeViewWithIdentifier:@"BTFCellView" owner:self];
	if (cellView == nil) {
		cellView = [[NSTableCellView alloc] initWithFrame:NSZeroRect];
		NSTextField *textField = [[NSTextField alloc] initWithFrame:NSZeroRect];
		[textField setBezeled:NO];
		[textField setDrawsBackground:NO];
		[textField setEditable:NO];
		[textField setSelectable:NO];
		[cellView addSubview:textField];
		cellView.textField = textField;
//		if ([self.BTFDelegate respondsToSelector:@selector(textView:imageForCompletion:)]) {
//			NSImageView *imageView = [[NSImageView alloc] initWithFrame:NSZeroRect];
//			[imageView setImageFrameStyle:NSImageFrameNone];
//			[imageView setImageScaling:NSImageScaleNone];
//			[cellView addSubview:imageView];
//			cellView.imageView = imageView;
//		}
		
		cellView.identifier = @"BTFCellView";
	}
	
	NSDictionary *attributes = @{NSFontAttributeName:BTF_POPOVER_FONT, NSForegroundColorAttributeName:BTF_POPOVER_TEXTCOLOR};
	NSMutableAttributedString *s = [[NSMutableAttributedString alloc] initWithString:matches[row] attributes:attributes];
	
	if (substring) {
		NSRange range = [s.string rangeOfString:substring options:NSAnchoredSearch|NSCaseInsensitiveSearch];
		[s addAttribute:NSFontAttributeName value:BTF_POPOVER_BOLDFOND range:range];
	}
	
	[cellView.textField setAttributedStringValue:s];
	return cellView;
}

- (NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row {
	return [[BTFAutocompleteTableRowView alloc] init];
}

@end

