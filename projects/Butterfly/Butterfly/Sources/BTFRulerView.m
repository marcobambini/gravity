//
//  BTFRulerView.m
//  Butterfly
//
//  Created by Marco Bambini on 15/07/15.
//  Copyright (c) 2015 Marco Bambini. All rights reserved.
//

#import "BTFRulerView.h"
#import "BTFTextView.h"

#define BTF_RULER_WIDTH		40.0f
#define BTF_RULER_PADDING	 5.0f

#define MinX(FRAME)			CGRectGetMinX(FRAME)
#define MinY(FRAME)			CGRectGetMinY(FRAME)
#define MaxX(FRAME)			CGRectGetMaxX(FRAME)
#define MaxY(FRAME)			CGRectGetMaxY(FRAME)

static inline void drawLineNumberInRect(NSUInteger lineNumber, NSRect lineRect, NSDictionary *attributes, CGFloat ruleThickness) {
	NSString *string = [[NSNumber numberWithUnsignedInteger:lineNumber] stringValue];
	NSAttributedString *attString = [[NSAttributedString alloc] initWithString:string attributes:attributes];
	NSUInteger x = ruleThickness - BTF_RULER_PADDING - attString.size.width;
	
	// Offsetting the drawing keeping into account the ascender (because we draw it without NSStringDrawingUsesLineFragmentOrigin)
	NSFont *font = attributes[NSFontAttributeName];
	lineRect.origin.x = x;
	lineRect.origin.y += font.ascender;
	
	[attString drawWithRect:lineRect options:0 context:nil];
}

static inline NSUInteger countNewLines(NSString *s, NSUInteger location, NSUInteger length) {
	CFStringInlineBuffer inlineBuffer;
	CFStringInitInlineBuffer((__bridge CFStringRef)s, &inlineBuffer, CFRangeMake(location, length));
	
	NSUInteger counter = 0;
	for (CFIndex i=0; i < length; ++i) {
		UniChar c = CFStringGetCharacterFromInlineBuffer(&inlineBuffer, i);
		if (c == (UniChar)'\n') ++counter;
	}
	return counter;
}

@implementation BTFRulerView

- (instancetype)initWithBTFTextView:(BTFTextView *)textView {
	self = [super initWithScrollView:textView.enclosingScrollView orientation:NSVerticalRuler];
	if (self) {
		self.clientView = textView;
		
		// default settings
		self.ruleThickness = BTF_RULER_WIDTH;
		self.textColor = [NSColor grayColor];
	}
	return self;
}

- (void)drawHashMarksAndLabelsInRect:(NSRect)rect {
	// BACKGROUND
	if (_backgroundColor) {
		// do not use drawBackgroundInRect for background color otherwise a 1px right border with a different color appears
		[_backgroundColor set];
		[NSBezierPath fillRect:rect];
	}
		
	if (_borderColor && _borderWidth > 0.0) {
		NSBezierPath *borderBezierPath = [NSBezierPath bezierPath];
		// LEFT
		[borderBezierPath moveToPoint:NSMakePoint(NSMinX(rect), MinY(rect))];
		[borderBezierPath lineToPoint:NSMakePoint(NSMinX(rect), MaxY(rect))];
		
		// RIGHT
		[borderBezierPath moveToPoint:NSMakePoint(MaxX(rect), MinY(rect))];
		[borderBezierPath lineToPoint:NSMakePoint(MaxX(rect), MaxY(rect))];
		
		[_borderColor setStroke];
		[borderBezierPath setLineWidth:_borderWidth];
		[borderBezierPath stroke];
	}
	
	// MARKS AND LABELS
	BTFTextView *textView = (BTFTextView *)self.clientView;
	if (!textView) return;
	
	NSLayoutManager *layoutManager = textView.layoutManager;
	if (!layoutManager) return;
	
	NSString *textString = textView.string;
	if ((!textString) || (textString.length == 0)) return;
	
	CGFloat insetHeight = textView.textContainerInset.height;
	CGPoint relativePoint = [self convertPoint:NSZeroPoint fromView:textView];
	
	// Gettign text attributes from the textview
	NSMutableDictionary *lineNumberAttributes = [[textView.textStorage attributesAtIndex:0 effectiveRange:NULL] mutableCopy];
	lineNumberAttributes[NSForegroundColorAttributeName] = self.textColor;
	
	NSRange visibleGlyphRange = [layoutManager glyphRangeForBoundingRect:textView.visibleRect inTextContainer:textView.textContainer];
	NSUInteger firstVisibleGlyphCharacterIndex = [layoutManager characterIndexForGlyphAtIndex:visibleGlyphRange.location];
	
	// line number for the first visible line
	NSUInteger lineNumber = countNewLines(textString, 0, firstVisibleGlyphCharacterIndex)+1;
	NSUInteger glyphIndexForStringLine = visibleGlyphRange.location;
	
	// go through each line in the string
	while (glyphIndexForStringLine < NSMaxRange(visibleGlyphRange)) {
		// range of current line in the string
		NSRange characterRangeForStringLine = [textString lineRangeForRange:NSMakeRange([layoutManager characterIndexForGlyphAtIndex:glyphIndexForStringLine], 0)];
		NSRange glyphRangeForStringLine = [layoutManager glyphRangeForCharacterRange: characterRangeForStringLine actualCharacterRange:nil];
		
		NSUInteger glyphIndexForGlyphLine = glyphIndexForStringLine;
		NSUInteger glyphLineCount = 0;
		
		while (glyphIndexForGlyphLine < NSMaxRange(glyphRangeForStringLine)) {
			// check if the current line in the string spread across several lines of glyphs
			NSRange effectiveRange = NSMakeRange(0, 0);
			
			// range of current "line of glyphs". If a line is wrapped then it will have more than one "line of glyphs"
			NSRect lineRect = [layoutManager lineFragmentRectForGlyphAtIndex:glyphIndexForGlyphLine effectiveRange:&effectiveRange withoutAdditionalLayout:YES];
			
			// compute Y for line number
			CGFloat y = ceil(NSMinY(lineRect) + relativePoint.y + insetHeight);
			lineRect.origin.y = y;
			
			// draw line number only if string does not spread across several lines
			if (glyphLineCount == 0) {
				drawLineNumberInRect(lineNumber, lineRect, lineNumberAttributes, self.ruleThickness);
			}
			
			// move to next glyph line
			++glyphLineCount;
			glyphIndexForGlyphLine = NSMaxRange(effectiveRange);
		}
		
		glyphIndexForStringLine = NSMaxRange(glyphRangeForStringLine);
		++lineNumber;
	}
	
	// draw line number for the extra line at the end of the text
	if (layoutManager.extraLineFragmentTextContainer) {
		NSRect lineRect = layoutManager.extraLineFragmentRect;
		CGFloat y = ceil(NSMinY(lineRect) + relativePoint.y + insetHeight);
		lineRect.origin.y = y;
		drawLineNumberInRect(lineNumber, lineRect, lineNumberAttributes, self.ruleThickness);
	}
}

@end

