; textobjects.scm — `miX` / `maX` selections in Helix,
; `af/if/ac/ic` etc. via nvim-treesitter-textobjects in Neovim.

; ── block constructs as functions ──
(meta_block) @function.around
(heading) @function.around
(paragraph) @function.around
(codeblock) @function.around
(hr) @function.around
(list_block) @function.around
(list_item) @function.around
(task_item) @function.around
(quote_block) @function.around
(image_block) @function.around
(table_block) @function.around
(table_row) @function.around
(table_cell) @function.around
(callout_block) @function.around
(raw_block) @function.around
(math_block) @function.around
(toc_block) @function.around
(footnote_def) @function.around
(include_stmt) @function.around

; ── inline constructs as functions ──
(bold) @function.around
(italic) @function.around
(strike) @function.around
(inline_code) @function.around
(inline_math) @function.around
(link) @function.around
(footnote_ref) @function.around

; ── attributes as parameters (`,wip` motions) ──
(named_arg) @parameter.around
(positional_arg) @parameter.around
(meta_field) @parameter.around

; ── comments ──
(comment) @comment.around
