/**
 * tree-sitter-docup — tree-sitter grammar for DocUP (.du)
 *
 * DocUP (Document Unambiguous Precise) compiler: docup-lang 0.3.1 (Rust).
 * Reference implementation: ../docup/src/{lexer,parser,ast}.rs
 *
 * Authoring: this file (JavaScript DSL, mandatory) + src/scanner.c (C, mandatory).
 * Generated: src/parser.c via `tree-sitter generate` (CLI 0.25.x, ABI 14).
 *
 * Design notes:
 * - `extras` (whitespace + comments) are global. The external scanner consumes
 *   raw/code/math payloads verbatim, so line and block comments inside
 *   verbatim scopes, `code{}` and `m{}` are content, not comments.
 *   (Matches lexer.rs: comments are token-mode only.)
 * - Prose text (`p`, `h`, `callout`, `cell`, `footnote`, item/quote bodies) is
 *   an external token (`_prose_text`). The scanner stops before `}`,
 *   backslash escapes, and inline openers (`b{`, `i{`, `code{`, `strike{`,
 *   `m{`, `link(`, `fn(`), so LR(1) never has to look ahead through text.
 * - Verbatim (`{! !}`), `code{balanced}` and `m{balanced}` are external
 *   tokens. Escape rules mirror lexer.rs exactly.
 * - tree-sitter parses structure; it does NOT validate semantics
 *   (duplicate meta, h(1..6), callout kind, table width, src-vs-body,
 *   nested-list-must-be-last). The Rust compiler owns those errors.
 */

// NOTE: external token order here MUST match src/scanner.c enum order.
// Prose text comes in three flavours because nested blocks are only legal
// in specific bodies (mirrors docup/src/parser.rs):
//   _prose_text — p/h/callout/cell/footnote (no nested blocks; any
//                 `list`/`quote` word is literal text)
//   _item_text  — item/task bodies (stops before nested `list(`/`list {`)
//   _quote_text — quote bodies (stops before nested `quote {`)

module.exports = grammar({
  name: 'docup',

  // ABI 14 — supported by CLI 0.25.x, Neovim, Helix, Zed, web-tree-sitter.
  abi: 14,

  extras: ($) => [/\s/, $.comment],

  // Lets tree-sitter extract `meta h p b i code strike m link fn list item
  // task quote image table row cell callout raw math toc footnote include`
  // as keywords for highlighting instead of generic identifiers.
  word: ($) => $.identifier,

  externals: ($) => [
    $._prose_text,
    $._raw_content,
    $._code_content,
    $._math_content,
    $._item_text,
    $._quote_text,
  ],

  // `Supertypes` help queries: every top-level construct is a `_block`,
  // every prose child is `_inline`.
  supertypes: ($) => [$._block, $._inline],

  // No `conflicts` needed: positional-vs-named args and prose-vs-inline
  // are resolved by longest-match + external-scanner token boundaries.

  rules: {
    // ── document ──────────────────────────────────────────────
    // lexer.rs/parser.rs: meta? block* EOF. Tree-sitter adds EOF implicitly.
    document: ($) => seq(optional($.meta_block), repeat($._block)),

    _block: ($) =>
      choice(
        $.heading,
        $.paragraph,
        $.codeblock,
        $.hr,
        $.list_block,
        $.quote_block,
        $.image_block,
        $.table_block,
        $.callout_block,
        $.raw_block,
        $.math_block,
        $.toc_block,
        $.footnote_def,
        $.include_stmt,
      ),

    // ── meta ──────────────────────────────────────────────────
    // meta { key: "value", ... } — keys Ident, values String only.
    meta_block: ($) =>
      seq('meta', '{', repeat(seq($.meta_field, optional(','))), '}'),

    meta_field: ($) =>
      seq(field('key', $.identifier), ':', field('value', $.string)),

    // ── headings & paragraphs ─────────────────────────────────
    // h(1) { ... } / h(2, id: "x", class: "y") { ... }
    heading: ($) =>
      seq(
        'h',
        '(',
        field('level', $.number),
        repeat(seq(',', $.named_arg)),
        ')',
        '{',
        optional($._prose),
        '}',
      ),

    paragraph: ($) => seq('p', '{', optional($._prose), '}'),

    callout_block: ($) =>
      seq(
        'callout',
        optional($.attr_list),
        '{',
        optional($._prose),
        '}',
      ),

    footnote_def: ($) =>
      seq('footnote', $.attr_list, '{', optional($._prose), '}'),

    // ── verbatim blocks ───────────────────────────────────────
    // codeblock(lang: "go", file: "main.go", src: "a.go",
    //           line_numbers: true, highlight: "1,3-5") {! ... !}
    // Body is optional: `codeblock(src: "f")` has no body. A plain
    // `codeblock { }` is intentionally NOT offered (parse error, like rustc).
    codeblock: ($) =>
      seq('codeblock', optional($.attr_list), optional($.raw_body)),

    raw_body: ($) => seq('{!', optional($._raw_content), '!}'),

    raw_block: ($) => seq('raw', '{!', optional($._raw_content), '!}'),

    math_block: ($) => seq('math', '{!', optional($._raw_content), '!}'),

    hr: ($) => seq('hr', '{', '}'),

    toc_block: ($) => seq('toc', '{', '}'),

    // ── lists ─────────────────────────────────────────────────
    // list { item { } task(done: true) { } }
    // list(ordered: true) { ... }, nestable via item body.
    // tree-sitter accepts nesting anywhere; "nested must be last"
    // is a semantic rule enforced by docup/src/parser.rs + sema.rs.
    list_block: ($) =>
      seq(
        'list',
        optional($.attr_list),
        '{',
        repeat1(choice($.list_item, $.task_item)),
        '}',
      ),

    list_item: ($) => seq('item', '{', optional($._item_body), '}'),

    task_item: ($) =>
      seq('task', optional($.attr_list), '{', optional($._item_body), '}'),

    // Permissive superset: inlines + text + nested lists anywhere.
    // "Nested must be last" is enforced by docup sema, not the grammar.
    _item_body: ($) =>
      repeat1(
        choice($._inline, $._item_text, $.escape_sequence, $.list_block),
      ),

    // ── quotes ────────────────────────────────────────────────
    // quote { text quote { nested } } — same "must be last" note as lists.
    quote_block: ($) =>
      seq(
        'quote',
        '{',
        repeat(
          choice($._inline, $._quote_text, $.escape_sequence, $.quote_block),
        ),
        '}',
      ),

    // ── media & tables ────────────────────────────────────────
    // image("src.png", alt: "...") — no body. Extra named args ignored
    // by the compiler; grammar keeps them for forward compatibility.
    image_block: ($) =>
      seq('image', '(', $.string, repeat(seq(',', $.named_arg)), ')'),

    table_block: ($) =>
      seq('table', optional($.attr_list), '{', repeat1($.table_row), '}'),

    table_row: ($) =>
      seq('row', optional($.attr_list), '{', repeat1($.table_cell), '}'),

    table_cell: ($) => seq('cell', '{', optional($._prose), '}'),

    // ── include ───────────────────────────────────────────────
    // include "file.du" == include("file.du")
    include_stmt: ($) =>
      seq('include', choice($.string, seq('(', $.string, ')'))),

    // ── attribute lists ───────────────────────────────────────
    // (key: "val", "positional", flag: true, n: 1)
    // Heading level `h(1, ...)` is NOT part of attr_list (it is $.number).
    attr_list: ($) =>
      seq(
        '(',
        optional(seq($._attr, repeat(seq(',', $._attr)), optional(','))),
        ')',
      ),

    _attr: ($) => choice($.positional_arg, $.named_arg),

    positional_arg: ($) => $.string,

    named_arg: ($) =>
      seq(field('key', $.identifier), ':', field('value', $.attr_value)),

    attr_value: ($) => choice($.string, $.boolean, $.number),

    // ── prose & inlines ───────────────────────────────────────
    // Only b/i/strike/code/m/link/fn open inlines. Any other
    // `word{` / `word(` in prose is plain text (scanner rule).
    _prose: ($) => repeat1(choice($._inline, $._prose_text, $.escape_sequence)),

    _inline: ($) =>
      choice(
        $.bold,
        $.italic,
        $.strike,
        $.inline_code,
        $.inline_math,
        $.link,
        $.footnote_ref,
      ),

    bold: ($) => seq('b', '{', optional($._prose), '}'),

    italic: ($) => seq('i', '{', optional($._prose), '}'),

    strike: ($) => seq('strike', '{', optional($._prose), '}'),

    // code{let x = 1;} — balanced braces, no nested inlines.
    // \{ \} \\ unescaped without changing depth (lexer.rs).
    inline_code: ($) => seq('code', '{', optional($._code_content), '}'),

    // m{E=mc^2} — balanced braces, \{ \} \\ preserved for KaTeX.
    inline_math: ($) => seq('m', '{', optional($._math_content), '}'),

    // link("https://example.com"){text} — extra named args allowed.
    link: ($) =>
      seq(
        'link',
        '(',
        field('url', $.string),
        repeat(seq(',', $.named_arg)),
        ')',
        '{',
        optional($._prose),
        '}',
      ),

    // fn(note-1) / fn("with space") — no body.
    footnote_ref: ($) =>
      seq('fn', '(', choice($.string, $.footnote_id), ')'),

    // ── terminals ─────────────────────────────────────────────
    // NOTE: `_prose_text`, `_item_text`, `_quote_text`, `_raw_content`,
    // `_code_content`, `_math_content` are EXTERNAL tokens — their bytes come
    // from src/scanner.c only. They are declared in `externals` above and
    // intentionally have NO rule bodies here (defining one would conflict
    // with the external scanner).

    // \{ \} \! \\ in prose (read_text_run_stopping_at, lexer.rs:487-520).
    escape_sequence: ($) => token(seq('\\', choice('{', '}', '!', '\\'))),

    // "..." with \" \\ \n \t \r; unknown \x kept as \+x (lexer.rs:221-284).
    string: ($) =>
      token(seq('"', repeat(choice(/[^"\\]+/, /\\./)), '"')),

    number: ($) => /[0-9]+/,

    boolean: ($) => choice('true', 'false'),

    identifier: ($) => /[A-Za-z_][A-Za-z0-9_]*/,

    // fn() ids allow hyphens: read_raw_ident (lexer.rs:419-426).
    footnote_id: ($) => /[A-Za-z_][A-Za-z0-9_\-]*/,

    // //line and /* block */ (non-nesting, first */ closes).
    // Valid everywhere except inside raw/code/math payloads, where the
    // external scanner swallows them as content (lexer.rs note).
    comment: ($) =>
      token(
        choice(seq('//', /[^\n]*/), seq('/*', /([^*]|\*[^/])*/, '*/')),
      ),
  },
});
