" Copyright (c) 2014 Keith Smiley (http://keith.so)
" Permission is hereby granted, free of charge, to any person obtaining a copy
" of this software and associated documentation files (the 'Software'), to deal
" in the Software without restriction, including without limitation the rights
" to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
" copies of " the Software, and to permit persons to whom the Software is
" furnished to do so, subject to the following conditions:

" The above copyright notice and this permission notice shall be included in all
" copies or substantial portions of the Software.

" THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
" IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
" FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
" AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
" LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
" OUT OF OR IN " CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
" THE SOFTWARE.

" File: gravity.vim
" Author: Keith Smiley
" Description: Runtime files for Gravity
" Last Modified: June 15, 2014

if exists("b:current_syntax")
  finish
endif

" Comments
" Shebang
syntax match gravityShebang "\v#!.*$"

" Comment contained keywords
syntax keyword gravityTodos contained TODO XXX FIXME NOTE
syntax keyword gravityMarker contained MARK

" In comment identifiers
function! s:CommentKeywordMatch(keyword)
  execute "syntax match gravityDocString \"\\v^\\s*-\\s*". a:keyword . "\\W\"hs=s+1,he=e-1 contained"
endfunction

syntax case ignore

call s:CommentKeywordMatch("attention")
call s:CommentKeywordMatch("author")
call s:CommentKeywordMatch("authors")
call s:CommentKeywordMatch("bug")
call s:CommentKeywordMatch("complexity")
call s:CommentKeywordMatch("copyright")
call s:CommentKeywordMatch("date")
call s:CommentKeywordMatch("experiment")
call s:CommentKeywordMatch("important")
call s:CommentKeywordMatch("invariant")
call s:CommentKeywordMatch("note")
call s:CommentKeywordMatch("parameter")
call s:CommentKeywordMatch("postcondition")
call s:CommentKeywordMatch("precondition")
call s:CommentKeywordMatch("remark")
call s:CommentKeywordMatch("remarks")
call s:CommentKeywordMatch("requires")
call s:CommentKeywordMatch("returns")
call s:CommentKeywordMatch("see")
call s:CommentKeywordMatch("since")
call s:CommentKeywordMatch("throws")
call s:CommentKeywordMatch("todo")
call s:CommentKeywordMatch("version")
call s:CommentKeywordMatch("warning")

syntax case match
delfunction s:CommentKeywordMatch


" Literals
" Strings
syntax region gravityString start=/"/ skip=/\\\\\|\\"/ end=/"/ contains=gravityInterpolatedWrapper oneline
syntax region gravityInterpolatedWrapper start="\v[^\\]\zs\\\(\s*" end="\v\s*\)" contained containedin=gravityString contains=gravityInterpolatedString,gravityString oneline
syntax match gravityInterpolatedString "\v\w+(\(\))?" contained containedin=gravityInterpolatedWrapper oneline

" Numbers
syntax match gravityNumber "\v<\d+>"
syntax match gravityNumber "\v<(\d+_+)+\d+(\.\d+(_+\d+)*)?>"
syntax match gravityNumber "\v<\d+\.\d+>"
syntax match gravityNumber "\v<\d*\.?\d+([Ee]-?)?\d+>"
syntax match gravityNumber "\v<0x[[:xdigit:]_]+([Pp]-?)?\x+>"
syntax match gravityNumber "\v<0b[01_]+>"
syntax match gravityNumber "\v<0o[0-7_]+>"

" BOOLs
syntax keyword gravityBoolean
      \ true
      \ false


" Operators
syntax match gravityOperator "\v\~"
syntax match gravityOperator "\v\s+!"
syntax match gravityOperator "\v\%"
syntax match gravityOperator "\v\^"
syntax match gravityOperator "\v\&"
syntax match gravityOperator "\v\*"
syntax match gravityOperator "\v-"
syntax match gravityOperator "\v\+"
syntax match gravityOperator "\v\="
syntax match gravityOperator "\v\|"
syntax match gravityOperator "\v\/"
syntax match gravityOperator "\v\."
syntax match gravityOperator "\v\<"
syntax match gravityOperator "\v\>"
syntax match gravityOperator "\v\?\?"

" Methods/Functions/Properties
syntax match gravityMethod "\(\.\)\@<=\w\+\((\)\@="
syntax match gravityProperty "\(\.\)\@<=\<\w\+\>(\@!"

" Gravity closure arguments
syntax match gravityClosureArgument "\$\d\+\(\.\d\+\)\?"

syntax match gravityAvailability "\v((\*(\s*,\s*[a-zA-Z="0-9.]+)*)|(\w+\s+\d+(\.\d+(.\d+)?)?\s*,\s*)+\*)" contains=gravityString
syntax keyword gravityPlatforms OSX iOS watchOS OSXApplicationExtension iOSApplicationExtension contained containedin=gravityAvailability
syntax keyword gravityAvailabilityArg renamed unavailable introduced deprecated obsoleted message contained containedin=gravityAvailability

" Keywords {{{
syntax keyword gravityKeywords
      \ associatedtype
      \ associativity
      \ atexit
      \ break
      \ case
      \ catch
      \ class
      \ continue
      \ convenience
      \ default
      \ defer
      \ deinit
      \ didSet
      \ do
      \ dynamic
      \ else
      \ extension
      \ fallthrough
      \ fileprivate
      \ final
      \ for
      \ func
      \ get
      \ guard
      \ if
      \ import
      \ in
      \ infix
      \ init
      \ inout
      \ internal
      \ lazy
      \ let
      \ mutating
      \ nil
      \ nonmutating
      \ operator
      \ optional
      \ override
      \ postfix
      \ precedence
      \ precedencegroup
      \ prefix
      \ private
      \ protocol
      \ public
      \ repeat
      \ required
      \ rethrows
      \ return
      \ self
      \ set
      \ static
      \ subscript
      \ super
      \ switch
      \ throw
      \ throws
      \ try
      \ typealias
      \ unowned
      \ var
      \ weak
      \ where
      \ while
      \ willSet

syntax match gravityMultiwordKeywords "indirect case"
syntax match gravityMultiwordKeywords "indirect enum"
" }}}

" Names surrounded by backticks. This aren't limited to keywords because 1)
" Gravity doesn't limit them to keywords and 2) I couldn't make the keywords not
" highlight at the same time
syntax region gravityEscapedReservedWord start="`" end="`" oneline

syntax keyword gravityAttributes
      \ @assignment
      \ @autoclosure
      \ @available
      \ @convention
      \ @discardableResult
      \ @exported
      \ @IBAction
      \ @IBDesignable
      \ @IBInspectable
      \ @IBOutlet
      \ @noescape
      \ @nonobjc
      \ @noreturn
      \ @NSApplicationMain
      \ @NSCopying
      \ @NSManaged
      \ @objc
      \ @testable
      \ @UIApplicationMain
      \ @warn_unused_result

syntax keyword gravityConditionStatement #available

syntax keyword gravityStructure
      \ struct
      \ enum

syntax keyword gravityDebugIdentifier
      \ #column
      \ #file
      \ #function
      \ #line
      \ __COLUMN__
      \ __FILE__
      \ __FUNCTION__
      \ __LINE__

syntax keyword gravityLineDirective #setline

syntax region gravityTypeWrapper start="\v:\s*" skip="\s*,\s*$*\s*" end="$\|/"me=e-1 contains=ALLBUT,gravityInterpolatedWrapper transparent
syntax region gravityTypeCastWrapper start="\(as\|is\)\(!\|?\)\=\s\+" end="\v(\s|$|\{)" contains=gravityType,gravityCastKeyword keepend transparent oneline
syntax region gravityGenericsWrapper start="\v\<" end="\v\>" contains=gravityType transparent oneline
syntax region gravityLiteralWrapper start="\v\=\s*" skip="\v[^\[\]]\(\)" end="\v(\[\]|\(\))" contains=ALL transparent oneline
syntax region gravityReturnWrapper start="\v-\>\s*" end="\v(\{|$)" contains=gravityType transparent oneline
syntax match gravityType "\v<\u\w*" contained containedin=gravityTypeWrapper,gravityLiteralWrapper,gravityGenericsWrapper,gravityTypeCastWrapper

syntax keyword gravityImports import
syntax keyword gravityCastKeyword is as contained

" 'preprocesor' stuff
syntax keyword gravityPreprocessor
      \ #if
      \ #elseif
      \ #else
      \ #endif
      \ #selector


" Comment patterns
syntax match gravityComment "\v\/\/.*$"
      \ contains=gravityTodos,gravityDocString,gravityMarker,@Spell oneline
syntax region gravityComment start="/\*" end="\*/"
      \ contains=gravityTodos,gravityDocString,gravityMarker,gravityComment,@Spell fold


" Set highlights
highlight default link gravityTodos Todo
highlight default link gravityDocString String
highlight default link gravityShebang Comment
highlight default link gravityComment Comment
highlight default link gravityMarker Comment

highlight default link gravityString String
highlight default link gravityInterpolatedWrapper Delimiter
highlight default link gravityNumber Number
highlight default link gravityBoolean Boolean

highlight default link gravityOperator Operator
highlight default link gravityCastKeyword Keyword
highlight default link gravityKeywords Keyword
highlight default link gravityMultiwordKeywords Keyword
highlight default link gravityEscapedReservedWord Normal
highlight default link gravityClosureArgument Operator
highlight default link gravityAttributes PreProc
highlight default link gravityConditionStatement PreProc
highlight default link gravityStructure Structure
highlight default link gravityType Type
highlight default link gravityImports Include
highlight default link gravityPreprocessor PreProc
highlight default link gravityMethod Function
highlight default link gravityProperty Identifier

highlight default link gravityConditionStatement PreProc
highlight default link gravityAvailability Normal
highlight default link gravityAvailabilityArg Normal
highlight default link gravityPlatforms Keyword
highlight default link gravityDebugIdentifier PreProc
highlight default link gravityLineDirective PreProc

" Force vim to sync at least x lines. This solves the multiline comment not
" being highlighted issue
syn sync minlines=100

let b:current_syntax = "gravity"
