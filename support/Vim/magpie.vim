" Vim syntax file
" Language:         Magpie
" Maintainer:       Paul Woolcock <pwoolcoc@gmail.com>
" Latest Revision:  11 January 2013


let s:cpo_save = &cpo
set cpo&vim

if version < 600
    syntax clear
elseif exists("b:current_syntax")
    finish
endif

syn keyword magpieSyntaxElement and async break case catch
syn keyword magpieSyntaxElement def defclass nextgroup=magpieFunction skipwhite
syn keyword magpieSyntaxElement do end else
syn keyword magpieSyntaxElement false fn for if import in is
syn keyword magpieSyntaxElement match not nothin or return then
syn keyword magpieSyntaxElement throw true val var while xor

syn match magpieFunction
      \ "\%(\%(def\s\|defclass\s\|@\)\s*\)\@<=\h\%(\w\|\.\)*" contained

"syn match magpiePattern 

hi def link magpieSyntaxElement Statement

let b:current_snytax = "magpie"

let &cpo = s:cpo_save
unlet s:cpo_save
