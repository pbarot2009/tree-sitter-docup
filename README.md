# tree-sitter-docup

tree-sitter grammar for **DocUP** (`.du`) — Document Unambiguous Precise.

Reference compiler: [`../docup/quote(`.du` -> HTML5, Rust `docup-lang 0.3.1`).
This grammar mirrors `docup/src/lexer.rs` + `parser.rs` + `ast.rs` exactly.

## Layout

```
tree-sitter-docup/
  grammar.js            # single source of truth (JS DSL, mandatory)
  src/scanner.c         # external scanner: prose/raw/code/math (C, mandatory)
  src/parser.c          # generated — do not edit (`tree-sitter generate`)
  queries/              # highlights.scm injections.scm folds.scm indents.scm
  test/corpus/          # `tree-sitter test` corpus
  examples/hello.du     # minimal sample
  bindings/{rust,python,node}/
```

## Authoring vs consuming (read this first)

| Layer | Language | Notes |
|---|---|---|
| `grammar.js` | JavaScript DSL | **Only choice.** `tree-sitter generate` compiles it. |
| `src/scanner.c` | C | Handles what LR(1) can't: prose runs, `{! !}`, balanced `code{}`/`m{}`. |
| `src/parser.c` | Generated C | ABI 14. Never edit. |
| Rust use | `tree-sitter` crate | Best for DocUP compiler (`docup-lang` is Rust). |
| Editor use | JS/WASM | `web-tree-sitter`, VSCode/Neovim/Helix/Zed via `queries/`. |
| Scripting | Python/Go | `py-tree-sitter`, `go-tree-sitter` over the same `parser.c`. |

Top 3 for DocUP: **1. Rust** (compiler integration) → **2. JS+WASM**
(editors) → **3. C/Python** (speed/scripting). Maintain `grammar.js` once,
publish to crates.io + npm.

## Quickstart

```bash
npm install -g tree-sitter-cli   # 0.25.x
cd tree-sitter-docup
tree-sitter generate             # grammar.js + scanner.c -> src/parser.c
tree-sitter test                 # corpus in test/corpus/
tree-sitter parse examples/hello.du
tree-sitter parse ../docup/samples/test.du
```

Rust:

```rust
let lang = tree_sitter_docup::language();
let mut p = tree_sitter::Parser::new();
p.set_language(&lang).unwrap();
let tree = p.parse("p { Hello b{DocUP} }", None).unwrap();
assert!(!tree.root_node().has_error());
```

## Grammar coverage

Blocks: `meta h p codeblock hr list item task quote image table row cell
callout raw math toc footnote include`.
Inlines (prose only): `b i strike code m link fn` + `\{ \} \! \\` escapes.
Verbatim: `{! !}` with `\!}` `!\}` `\\!}` escapes (same as `lexer.rs`).
Comments `//` `/* */` are `extras` between blocks; inside prose/raw/code/math
they are literal content (same as the Rust lexer).

What tree-sitter does NOT check (left to `docup` sema):
duplicate `meta`, `h(1..6)`, callout kind, table width, `src`-vs-body,
`item`/`quote` "nested must be last", empty verbatim, include cycles.

## Queries

- `highlights.scm` — keywords, strings, numbers, booleans, escapes, links.
- `injections.scm` — `codeblock(lang)` → that language, `m`/`math` → latex,
  `raw` → html, inline `code` → text.
- `folds.scm`, `indents.scm` — every braced block.
- `textobjects.scm` — blocks/inlines as functions, attrs as parameters.

## Helix setup (Helix 25.x)

1. Build the grammar shared library from this repo:

   ```bash
   cd tree-sitter-docup
   tree-sitter generate   # only needed after editing grammar.js / scanner.c
   tree-sitter build -o ~/.config/helix/runtime/grammars/docup.so .
   ```

2. Install the queries:

   ```bash
   mkdir -p ~/.config/helix/runtime/queries/docup
   cp queries/*.scm ~/.config/helix/runtime/queries/docup/
   ```

3. Register the language in `~/.config/helix/languages.toml`:

   ```toml
   [[language]]
   name = "docup"
   scope = "source.docup"
   file-types = ["du"]
   roots = [".git"]
   comment-token = "//"
   block-comment-tokens = { start = "/*", end = "*/" }
   indent = { tab-width = 4, unit = "    " }
   grammar = "docup"
   ```

4. Verify:

   ```bash
   hx --health docup   # want: Tree-sitter parser ✓, Highlight queries ✓
   hx some-file.du     # .du files now highlight
   ```

   After editing `grammar.js` or `src/scanner.c`, repeat steps 1–2
   (rebuild the `.so`, re-copy any changed `queries/*.scm`).

   Once this repo is pushed to GitHub, you can switch to managed grammars:

   ```toml
   [[grammar]]
   name = "docup"
   source = { git = "https://github.com/<you>/tree-sitter-docup", rev = "main" }
   ```

   then `hx --grammar fetch` + `hx --grammar build` maintain `docup.so`.
