" Vim syntax file
" Author:				pattern	
" Language:     joy2chord
" URL:          http://joy2chord.sourceforge.net
" Revision:     1.0

if version < 600
  syntax clear
elseif exists("b:current_syntax")
  " A syntax file has already been loaded
  finish
endi

syn case ignore

syn keyword j2cKeyword jsdev
syn keyword j2cKeyword total_simple_buttons
syn keyword j2cKeyword total_chorded_buttons
syn keyword j2cKeyword total_modes
syn keyword j2cKeyword total_macros
syn keyword j2cKeyword total_modifiers

syn region j2cComment start=/#/ end=/$/

" Note, this j2cNumber definition has to come *before* any other definition with
" numbers in them, as otherwise it would override them.
syn  match  j2cNumber        /\d\+/

syn  match  j2cChordButton   /^chord_b\d\+/
syn  match  j2cChord         /^\d\+chord\d\+/
syn  match  j2cDelimiter     /=/
syn  match  j2cKey           /KEY_.*/
syn  match  j2cModeCode      /^\d\+modecode/
syn  match  j2cModifier      /^\d\+modifier/
syn  match  j2cSimpleButton  /^simple_b\d\+/
syn  match  j2cSimple        /^\d\+simple\d\+/

syn sync fromstart

hi def link j2cChordButton  Keyword
hi def link j2cChord        Keyword
hi def link j2cComment      Comment
hi def link j2cDelimiter    Delimiter
hi def link j2cKey          String
hi def link j2cKeyword      Keyword
hi def link j2cModeCode     Keyword
hi def link j2cModifier     Keyword
hi def link j2cNumber       Number
hi def link j2cSimpleButton Keyword
hi def link j2cSimple       Keyword

let b:current_syntax = "j2c"
