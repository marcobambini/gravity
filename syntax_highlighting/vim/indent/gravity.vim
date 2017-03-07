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
" Description: The indent file for Gravity
" Last Modified: December 05, 2014

if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

let s:cpo_save = &cpo
set cpo&vim

setlocal nosmartindent
setlocal indentkeys-=e
setlocal indentkeys+=0]
setlocal indentexpr=GravityIndent()

function! s:NumberOfMatches(char, string, index)
  let instances = 0
  let i = 0
  while i < strlen(a:string)
    if a:string[i] == a:char && !s:IsExcludedFromIndentAtPosition(a:index, i + 1)
      let instances += 1
    endif

    let i += 1
  endwhile

  return instances
endfunction

function! s:SyntaxNameAtPosition(line, column)
  return synIDattr(synID(a:line, a:column, 0), "name")
endfunction

function! s:SyntaxName()
  return s:SyntaxNameAtPosition(line("."), col("."))
endfunction

function! s:IsExcludedFromIndentAtPosition(line, column)
  let name = s:SyntaxNameAtPosition(a:line, a:column)
  return name ==# "gravityComment" || name ==# "gravityString"
endfunction

function! s:IsExcludedFromIndent()
  return s:SyntaxName() ==# "gravityComment" || s:SyntaxName() ==# "gravityString"
endfunction

function! s:IsCommentLine(lnum)
    return synIDattr(synID(a:lnum,
          \     match(getline(a:lnum), "\S") + 1, 0), "name")
          \ ==# "gravityComment"
endfunction

function! GravityIndent(...)
  let clnum = a:0 ? a:1 : v:lnum

  let line = getline(clnum)
  let previousNum = prevnonblank(clnum - 1)
  while s:IsCommentLine(previousNum) != 0
    let previousNum = prevnonblank(previousNum - 1)
  endwhile

  let previous = getline(previousNum)
  let cindent = cindent(clnum)
  let previousIndent = indent(previousNum)

  let numOpenParens = s:NumberOfMatches("(", previous, previousNum)
  let numCloseParens = s:NumberOfMatches(")", previous, previousNum)
  let numOpenBrackets = s:NumberOfMatches("{", previous, previousNum)
  let numCloseBrackets = s:NumberOfMatches("}", previous, previousNum)

  let currentOpenBrackets = s:NumberOfMatches("{", line, clnum)
  let currentCloseBrackets = s:NumberOfMatches("}", line, clnum)

  let numOpenSquare = s:NumberOfMatches("[", previous, previousNum)
  let numCloseSquare = s:NumberOfMatches("]", previous, previousNum)

  let currentCloseSquare = s:NumberOfMatches("]", line, clnum)
  if numOpenSquare > numCloseSquare && currentCloseSquare < 1
    return previousIndent + shiftwidth()
  endif

  if currentCloseSquare > 0 && line !~ '\v\[.*\]'
    let column = col(".")
    call cursor(line("."), 1)
    let openingSquare = searchpair("\\[", "", "\\]", "bWn", "s:IsExcludedFromIndent()")
    call cursor(line("."), column)

    if openingSquare == 0
      return -1
    endif

    " - Line starts with closing square, indent as opening square
    if line =~ '\v^\s*]'
      return indent(openingSquare)
    endif

    " - Line contains closing square and more, indent a level above opening
    return indent(openingSquare) + shiftwidth()
  endif

  if line =~ ":$"
    let switch = search("switch", "bWn")
    return indent(switch)
  elseif previous =~ ":$"
    return previousIndent + shiftwidth()
  endif

  if numOpenParens == numCloseParens
    if numOpenBrackets > numCloseBrackets
      if currentCloseBrackets > currentOpenBrackets || line =~ "\\v^\\s*}"
        let column = col(".")
        call cursor(line("."), 1)
        let openingBracket = searchpair("{", "", "}", "bWn", "s:IsExcludedFromIndent()")
        call cursor(line("."), column)
        if openingBracket == 0
          return -1
        else
          return indent(openingBracket)
        endif
      endif

      return previousIndent + shiftwidth()
    elseif previous =~ "}.*{"
      if line =~ "\\v^\\s*}"
        return previousIndent
      endif

      return previousIndent + shiftwidth()
    elseif line =~ "}.*{"
      let openingBracket = searchpair("{", "", "}", "bWn", "s:IsExcludedFromIndent()")
      return indent(openingBracket)
    elseif currentCloseBrackets > currentOpenBrackets
      let column = col(".")
      call cursor(line("."), 1)
      let openingBracket = searchpair("{", "", "}", "bWn", "s:IsExcludedFromIndent()")
      call cursor(line("."), column)

      let bracketLine = getline(openingBracket)

      let numOpenParensBracketLine = s:NumberOfMatches("(", bracketLine, openingBracket)
      let numCloseParensBracketLine = s:NumberOfMatches(")", bracketLine, openingBracket)
      if numCloseParensBracketLine > numOpenParensBracketLine
        let line = line(".")
        let column = col(".")
        call cursor(openingParen, column)
        let openingParen = searchpair("(", "", ")", "bWn", "s:IsExcludedFromIndent()")
        call cursor(line, column)
        return indent(openingParen)
      endif
      return indent(openingBracket)
    else
      " - Current line is blank, and the user presses 'o'
      return previousIndent
    endif
  endif

  if numCloseParens > 0
    if currentOpenBrackets > 0 || currentCloseBrackets > 0
      if currentOpenBrackets > 0
        if numOpenBrackets > numCloseBrackets
          return previousIndent + shiftwidth()
        endif

        if line =~ "}.*{"
          let openingBracket = searchpair("{", "", "}", "bWn", "s:IsExcludedFromIndent()")
          return indent(openingBracket)
        endif

        if numCloseParens > numOpenParens
          let line = line(".")
          let column = col(".")
          call cursor(line - 1, column)
          let openingParen = searchpair("(", "", ")", "bWn", "s:IsExcludedFromIndent()")
          call cursor(line, column)
          return indent(openingParen)
        endif

        return previousIndent
      endif

      if currentCloseBrackets > 0
        let openingBracket = searchpair("{", "", "}", "bWn", "s:IsExcludedFromIndent()")
        return indent(openingBracket)
      endif

      return cindent
    endif

    if numCloseParens < numOpenParens
      if numOpenBrackets > numCloseBrackets
        return previousIndent + shiftwidth()
      endif

      let previousParen = match(previous, "(")
      return indent(previousParen) + shiftwidth()
    endif

    if numOpenBrackets > numCloseBrackets
      let line = line(".")
      let column = col(".")
      call cursor(previousNum, column)
      let openingParen = searchpair("(", "", ")", "bWn", "s:IsExcludedFromIndent()")
      call cursor(line, column)
      return indent(openingParen) + shiftwidth()
    endif

    " - Previous line has close then open braces, indent previous + 1 'sw'
    if previous =~ "}.*{"
      return previousIndent + shiftwidth()
    endif

    let line = line(".")
    let column = col(".")
    call cursor(previousNum, column)
    let openingParen = searchpair("(", "", ")", "bWn", "s:IsExcludedFromIndent()")
    call cursor(line, column)

    return indent(openingParen)
  endif

  " - Line above has (unmatched) open paren, next line needs indent
  if numOpenParens > 0
    let savePosition = getcurpos()
    " Must be at EOL because open paren has to be above (left of) the cursor
    call cursor(previousNum, col("$"))
    let previousParen = searchpair("(", "", ")", "bWn", "s:IsExcludedFromIndent()")
    call setpos(".", savePosition)
    return indent(previousParen) + shiftwidth()
  endif

  return cindent
endfunction

let &cpo = s:cpo_save
unlet s:cpo_save
