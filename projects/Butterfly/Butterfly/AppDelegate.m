//
//  AppDelegate.m
//  Butterfly
//
//  Created by Marco Bambini on 18/03/2017.
//  Copyright © 2017 Creolabs. All rights reserved.
//

#import "AppDelegate.h"
#import "BTFThemeKeys.h"
#import "BTFCodeEditor.h"
#import "gravity_token.h"

@interface AppDelegate () {
	BTFCodeEditor                  *codeEditor;
}

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet NSView   *codeEditorView;
@end

@implementation AppDelegate

// MARK: Utils functions

static NSColor *colorFromHexadecimalValue(NSString *hex) {
	if ([hex hasPrefix:@"#"]) {
		hex = [hex substringWithRange:NSMakeRange(1, [hex length] - 1)];
	}
	
	unsigned int colorCode = 0;
	
	// if string has only 6 characters append FF, opaque color
	if (hex.length == 6)
		hex = [NSString stringWithFormat:@"%@FF", hex];
	
	if (hex) {
		NSScanner *scanner = [NSScanner scannerWithString:hex];
		if (![scanner scanHexInt:&colorCode] || ! [scanner isAtEnd]) return nil;
	}
	
	return [NSColor colorWithCalibratedRed:((colorCode>>24)&0xFF)/255.0 green:((colorCode>>16)&0xFF)/255.0 blue:((colorCode>>8)&0xFF)/255.0 alpha:((colorCode)&0xFF)/255.0];
}

static NSDictionary *deserializeTheme(NSDictionary *theme) {
	NSMutableDictionary	*d = [[NSMutableDictionary alloc] initWithCapacity:theme.count];
	
	// ADJUST FONT
	CGFloat		fontSize = 13;
	NSString	*fontName = @"Menlo-Regular";
	NSString	*boldFontName = @"Menlo-Bold";
	NSFont		*fontRegular = nil;
	NSFont		*fontBold = nil;
	
	if (theme[BTFKEY_FONT_SIZE]) fontSize = ((NSNumber*)theme[BTFKEY_FONT_SIZE]).floatValue;
	if (theme[BTFKEY_FONT]) fontName = theme[BTFKEY_FONT];
	if (theme[BTFKEY_BOLD_FONT]) boldFontName = theme[BTFKEY_BOLD_FONT];
	fontRegular = [NSFont fontWithName:fontName size:fontSize];
	fontBold = [NSFont fontWithName:boldFontName size:fontSize];
	d[BTFKEY_FONT] = fontRegular;
	d[BTFKEY_BOLD_FONT] = fontBold;
	
	// ADJUST all other values
	for (NSString *key in theme.allKeys) {
		if ([key hasSuffix:@"COLOR"]) d[key] = colorFromHexadecimalValue(theme[key]);
		else if ([key hasSuffix:@"IMAGE"]) d[key] = [NSImage imageNamed:(theme[key])];
		else if ([key containsString:@"FONT"]) continue;
		else d[key] = theme[key];
	}
	
	return d;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	codeEditor = [[BTFCodeEditor alloc] initWithFrame:_codeEditorView.frame inView:_window];
	
	// apply theme (available themes are: Gauss, Laplace, Fourier and Riemann)
	NSURL *themeURL = [[NSBundle mainBundle] URLForResource:@"Gauss" withExtension:@"theme" subdirectory:@"Themes"];
	NSData *data = [NSData dataWithContentsOfURL:themeURL];
	NSDictionary *theme = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
	// it is caller responsability to propertly deserize theme
	[codeEditor applyTheme:deserializeTheme(theme)];
	
	// add reserved keywords to autocompletion engine
	// set reserved keywords
	uint32_t i, idx_end;
	token_keywords_indexes(&i, &idx_end);
	for (; i<=idx_end; ++i) {
		const char *keyword = token_name(i);
		if (keyword) [codeEditor addAutocompleteEntity:[NSString stringWithUTF8String:keyword] type:0];
	}
	
	// UTF-8 EXAMPLE
	codeEditor.string = @" // UTF-8 4 bytes\nvar a = \" 😀 a b\";\n\n// UTF-8 3 bytes\nvar b = \" 肉 a b\";\n\n// UTF-8 2 bytes\nvar c = \" © a b\";\n\n// UTF-8 1 byte\nvar d = \" a a b\";";
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
	// Insert code here to tear down your application
}


@end
