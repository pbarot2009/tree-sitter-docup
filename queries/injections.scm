; Inject inner languages into verbatim + inline payloads.
; (`tree-sitter test` runs the parser only; editors consume these.)

; codeblock bodies: highlighted per `lang` by the host editor.
; The `lang` string value selects the language (same idea as Typst raw lang).
(codeblock
  (attr_list
    (named_arg
      key: (identifier) @lang_key
      value: (attr_value
        (string) @injection.language)))
  (raw_body) @injection.content
  (#eq? @lang_key "lang"))

; codeblock without `lang` (e.g. src-only or plain) -> plain text.
((codeblock
   (raw_body) @injection.content)
 (#set! injection.language "text"))

; inline code{...} has no lang -> text.
((inline_code) @injection.content
 (#set! injection.language "text"))

; inline math m{...} and display math -> latex.
((inline_math) @injection.content
 (#set! injection.language "latex"))

((math_block) @injection.content
 (#set! injection.language "latex"))

; raw{!...!} is the unescaped HTML escape hatch -> html.
((raw_block) @injection.content
 (#set! injection.language "html"))
