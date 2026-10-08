; tree-sitter-docup highlights — Neovim / Helix / Zed
; Reference: ../docup/src/{codegen,highlight}.rs keyword set

(comment) @comment

; ── block keywords ──
"meta" @keyword
"h" @keyword
"p" @keyword
"codeblock" @keyword
"hr" @keyword
"list" @keyword
"item" @keyword
"task" @keyword
"quote" @keyword
"image" @keyword
"table" @keyword
"row" @keyword
"cell" @keyword
"callout" @keyword
"raw" @keyword
"math" @keyword
"toc" @keyword
"footnote" @keyword
"include" @keyword

; ── inline keywords ──
"b" @strong
"i" @italic
"strike" @strike
"code" @keyword
"m" @keyword
"link" @link
"fn" @keyword

; ── structure ──
(meta_field key: (identifier) @property)
(named_arg key: (identifier) @property)
(heading level: (number) @number)
(string) @string
(number) @number
(boolean) @boolean
(escape_sequence) @escape
(footnote_ref) @tag
(link url: (string) @link.uri)
(image_block (string) @link.uri)
(include_stmt (string) @string.special.path)
(raw_block) @markup.raw.block
(math_block) @markup.math
(codeblock) @markup.raw.block

; ── prose ──
(bold) @strong
(italic) @italic
(strike) @strike
(inline_code) @markup.raw.inline
(inline_math) @markup.math
(quote_block) @markup.quote
(list_block) @markup.list
(table_block) @markup.table
(callout_block) @markup.note
(heading) @markup.heading
(paragraph) @markup.paragraph
