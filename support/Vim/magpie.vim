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

syn keyword magpieSyntaxElement and or not xor
syn keyword magpieSyntaxElement def defclass fn nextgroup=magpieFunction skipwhite
syn keyword magpieSyntaxElement async break throw catch end
syn keyword magpieSyntaxElement if else then
syn keyword magpieSyntaxElement true false
syn keyword magpieSyntaxElement for while
syn keyword magpieSyntaxElement match val do var import in is nothing return case

syn match magpieFunction
      \ "\%(\%(def\s\|defclass\s\|@\)\s*\)\@<=\h\%(\w\|\.\)*" contained

"syn match magpiePattern 

hi def link magpieSyntaxElement Statement

let b:current_snytax = "magpie"

let &cpo = s:cpo_save
unlet s:cpo_save
