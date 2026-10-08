// tree-sitter-docup external scanner.
//
// Mirrors ../docup/src/lexer.rs exactly:
//   read_text_run_stopping_at  (lexer.rs:487-520) -> PROSE_TEXT
//   read_raw_until_bang_brace  (lexer.rs:297-351) -> RAW_CONTENT
//   read_balanced_braces       (lexer.rs:436-477) -> CODE_CONTENT / MATH_CONTENT
//
// Stateless scanner: no跨-token state, so serialize/deserialize are no-ops.
// Token order MUST match grammar.js `externals` array order.

#include "tree_sitter/parser.h"

#include <ctype.h>
#include <string.h>

enum TokenType {
  PROSE_TEXT,
  RAW_CONTENT,
  CODE_CONTENT,
  MATH_CONTENT,
  ITEM_TEXT,
  QUOTE_TEXT,
};

// Text flavours (see grammar.js note). Bitmask: which nested blocks end text.
typedef enum {
  TEXT_PLAIN = 0, // p/h/callout/cell/footnote — inline openers only
  TEXT_ITEM = 1, // item/task bodies — also stops before nested `list`
  TEXT_QUOTE = 2, // quote bodies — also stops before nested `quote`
} TextMode;

// ── scanner lifecycle (stateless) ────────────────────────────────────────────

void *tree_sitter_docup_external_scanner_create(void) { return NULL; }

void tree_sitter_docup_external_scanner_destroy(void *payload) {
  (void)payload;
}

unsigned tree_sitter_docup_external_scanner_serialize(void *payload,
                                                     char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_docup_external_scanner_deserialize(void *payload,
                                                   const char *buffer,
                                                   unsigned length) {
  (void)payload;
  (void)buffer;
  (void)length;
}

// ── helpers ──────────────────────────────────────────────────────────────────

static inline bool is_ident_start(int32_t c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static inline bool is_ident_cont(int32_t c) {
  return is_ident_start(c) || (c >= '0' && c <= '9');
}

// Check `word` (length `len`) against DocUP inline openers.
// Brace openers: b{ i{ code{ strike{ m{
// Paren openers: link( fn(
static bool is_brace_inline(const char *word, size_t len) {
  if (len == 1 && (word[0] == 'b' || word[0] == 'i' || word[0] == 'm')) {
    return true;
  }
  if (len == 4 && memcmp(word, "code", 4) == 0) {
    return true;
  }
  if (len == 6 && memcmp(word, "strike", 6) == 0) {
    return true;
  }
  return false;
}

static bool is_paren_inline(const char *word, size_t len) {
  if (len == 4 && memcmp(word, "link", 4) == 0) {
    return true;
  }
  if (len == 2 && memcmp(word, "fn", 2) == 0) {
    return true;
  }
  return false;
}

// ── PROSE / ITEM / QUOTE TEXT ────────────────────────────────────────────────
// Run of body-text chars. Stops (without consuming) before:
//   `}`                       end of block / inline
//   `\{` `\}` `\!` `\\`        escape_sequence (grammar token)
//   `b{` `i{` `code{` `strike{` `m{` `link(` `fn(`
// Plus, per TextMode (mirrors read_text_run_until_block + starts_block_after_
// ident, lexer.rs:483-531 — only ' ' and '\t' skipped, never '\n'):
//   TEXT_ITEM:  `list` + optional spaces/tabs + `(` or `{` (nested list)
//   TEXT_QUOTE: `quote` + optional spaces/tabs + `{` (nested quote)
// After a word was speculatively consumed, check for a nested block opener:
//   TEXT_ITEM:  `list` + spaces/tabs + `(` or `{`
//   TEXT_QUOTE: `quote` + spaces/tabs + `{`
// (Only ' ' and '\t' skipped — never '\n', like starts_block_after_ident.)
// Speculatively advances past spaces; on true the caller ends text WITHOUT
// marking (token ends before the word); on false the caller marks everything
// consumed so far as text and keeps scanning.
static bool is_block_opener(TSLexer *lexer, const char *word, size_t len,
                            bool overlong, TextMode mode) {
  if (overlong) {
    return false;
  }
  if (mode == TEXT_ITEM) {
    if (len != 4 || memcmp(word, "list", 4) != 0) {
      return false;
    }
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      lexer->advance(lexer, false);
    }
    return lexer->lookahead == '(' || lexer->lookahead == '{';
  }
  if (mode == TEXT_QUOTE) {
    if (len != 5 || memcmp(word, "quote", 5) != 0) {
      return false;
    }
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
      lexer->advance(lexer, false);
    }
    return lexer->lookahead == '{';
  }
  return false;
}

// Any other `word{` / `word(` is plain text (e.g. `list` inside p{}).
// Returns false when no text (lets grammar lex keyword / `}` / escape).
static bool scan_text(TSLexer *lexer, TextMode mode) {
  // No text if we start at end, `}`, or an escape.
  if (lexer->lookahead == 0 || lexer->lookahead == '}') {
    return false;
  }
  if (lexer->lookahead == '\\') {
    lexer->advance(lexer, false);
    int32_t n = lexer->lookahead;
    // Escapes \{ \} \! \\ belong to `escape_sequence`, not text.
    // NOTE: we already consumed `\`; tree-sitter has no un-consume except
    // via mark_end backtrack. We have NOT called mark_end yet, so returning
    // false rewinds to the scan start — the `\` stays for the grammar lexer.
    if (n == '{' || n == '}' || n == '!' || n == '\\') {
      return false;
    }
    // Lone backslash (e.g. `\n`, `\x`) is literal text in prose.
    // Fall through: `\` consumed, continue the run below.
  } else {
    // Cheap opener check at scan start: identifier immediately forming an
    // inline opener means zero-length text -> let the grammar match it.
    if (is_ident_start(lexer->lookahead)) {
      char word[16];
      size_t len = 0;
      // Speculatively consume the word to classify it.
      while (is_ident_cont(lexer->lookahead) && len < sizeof(word)) {
        word[len++] = (char)lexer->lookahead;
        lexer->advance(lexer, false);
      }
      // Word longer than buffer or followed by more ident chars: plain text
      // start (e.g. `boldly`), but we already consumed part of it. Since no
      // mark_end was set, returning true now would emit a partial token, so
      // instead rescan: report text by consuming one char at a time below.
      // Simplest correct path: if the word is overlong, treat scan as text.
      bool overlong = is_ident_cont(lexer->lookahead);
      int32_t after = lexer->lookahead;
      // No mark_end set -> token would start at scan start and end wherever
      // we next mark. We have not marked, so returning false rewinds fully
      // and the main loop below re-consumes correctly char by char.
      // Only shortcut when the word is a REAL inline or block opener.
      if (!overlong && len <= 6) {
        if ((is_brace_inline(word, len) && after == '{') ||
            (is_paren_inline(word, len) && after == '(')) {
          return false;
        }
      }
      if (is_block_opener(lexer, word, len, overlong, mode)) {
        return false;
      }
      // Not an opener: this is text. Continue scanning from after the word.
      // Mark everything consumed so far as text.
      lexer->mark_end(lexer);
    }
  }

  bool consumed_any = false;
  // If we entered via the lone-backslash branch, one char is consumed but
  // unmarked; mark it now if it is genuine text.
  if (lexer->lookahead != 0) {
    // Mark current progress (covers lone `\` + word-prefix paths).
    // Calling mark_end unconditionally here is safe: worst case the token
    // is the single char(s) consumed so far.
    lexer->mark_end(lexer);
    consumed_any = true;
  }

  for (;;) {
    int32_t c = lexer->lookahead;
    if (c == 0 || c == '}') {
      break;
    }
    if (c == '\\') {
      // Peek the char after `\` by consuming it.
      lexer->advance(lexer, false);
      int32_t n = lexer->lookahead;
      if (n == '{' || n == '}' || n == '!' || n == '\\') {
        // Escape starts here: end text BEFORE the `\`.
        // Last mark_end is before `\` (we mark only after safe chars),
        // so break without marking the `\`.
        // Edge: `\` was consumed but unmarked — since mark_end was last set
        // before it, the emitted token excludes it. Correct.
        // To rewind the consumed `\`, return true (token ends at last mark);
        // tree-sitter re-lexes from there for `escape_sequence`.
        break;
      }
      // Lone backslash: literal text, include both chars.
      lexer->mark_end(lexer);
      consumed_any = true;
      if (n == 0) {
        break;
      }
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      consumed_any = true;
      continue;
    }
    if (is_ident_start(c)) {
      // Gather the word to test for an inline opener.
      char word[16];
      size_t len = 0;
      // Remember: last mark_end is BEFORE this word. If it turns out to be
      // an opener and we already have text, break (token = text so far).
      // If it is an opener at token start, return false/true accordingly.
      while (is_ident_cont(lexer->lookahead) && len < sizeof(word)) {
        word[len++] = (char)lexer->lookahead;
        lexer->advance(lexer, false);
      }
      bool overlong = is_ident_cont(lexer->lookahead);
      int32_t after = lexer->lookahead;
      if (!overlong && len <= 6 &&
          ((is_brace_inline(word, len) && after == '{') ||
           (is_paren_inline(word, len) && after == '('))) {
        // Inline opener found. End text before it.
        break;
      }
      if (is_block_opener(lexer, word, len, overlong, mode)) {
        // Nested block opener found. End text before it (last mark_end is
        // before the word; spaces skipped by the check are discarded).
        break;
      }
      // Plain word (or overlong identifier): it is text.
      lexer->mark_end(lexer);
      consumed_any = true;
      continue;
    }
    // Ordinary char (spaces, punctuation, `(`, `{` not after inline word,
    // `/` of comments — comments inside prose are TEXT per lexer.rs).
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed_any = true;
  }

  if (!consumed_any) {
    return false;
  }
  // NOTE: do NOT mark_end here. Every consumed safe char is marked inline;
  // boundary chars (`b{` openers, `\` escapes, `}`) are consumed
  // speculatively and the last mark is BEFORE them. Marking now would swallow
  // the boundary into the text token.
  return true;
}

// ── RAW_CONTENT ──────────────────────────────────────────────────────────────
// Payload of `{! ... !}`. Stops before the first UNESCAPED `!}`.
// Escapes consumed as content: `\!}` `!\}` `\\!}` (lexer.rs:302-341).
// Returns false when empty (grammar `optional()` handles it) or on
// unterminated input (lets the parser produce an error node).
static bool scan_raw_content(TSLexer *lexer) {
  bool consumed_any = false;
  for (;;) {
    int32_t c = lexer->lookahead;
    if (c == 0) {
      return false; // unterminated — error node territory
    }
    if (c == '\\') {
      // \!}  or  \\!} ?
      lexer->advance(lexer, false); // consume `\`
      if (lexer->lookahead == '!' ) {
        lexer->advance(lexer, false); // consume `!`
        if (lexer->lookahead == '}') {
          lexer->advance(lexer, false); // consume `}`
          lexer->mark_end(lexer);
          consumed_any = true;
          continue; // escaped `\!}` -> content
        }
        lexer->mark_end(lexer);
        consumed_any = true;
        continue; // lone `\` + `!` -> content
      }
      if (lexer->lookahead == '\\') {
        lexer->advance(lexer, false); // consume second `\`
        if (lexer->lookahead == '!') {
          lexer->advance(lexer, false);
          if (lexer->lookahead == '}') {
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            consumed_any = true;
            continue; // `\\!}` -> content
          }
        }
        lexer->mark_end(lexer);
        consumed_any = true;
        continue;
      }
      lexer->mark_end(lexer);
      consumed_any = true;
      continue; // `\` + other -> content
    }
    if (c == '!') {
      lexer->advance(lexer, false); // consume `!`
      if (lexer->lookahead == '\\') {
        lexer->advance(lexer, false); // consume `\`
        if (lexer->lookahead == '}') {
          lexer->advance(lexer, false); // consume `}`
          lexer->mark_end(lexer);
          consumed_any = true;
          continue; // `!\}` -> content
        }
        lexer->mark_end(lexer);
        consumed_any = true;
        continue;
      }
      if (lexer->lookahead == '}') {
        // Unescaped `!}` terminator: end BEFORE `!`.
        // We consumed `!` speculatively; last mark_end is before it, so
        // returning now excludes it. Tree-sitter rewinds to the mark.
        if (!consumed_any) {
          return false; // empty payload
        }
        return true;
      }
      lexer->mark_end(lexer);
      consumed_any = true;
      continue; // `!` + other -> content
    }
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed_any = true;
  }
}

// ── CODE / MATH CONTENT ──────────────────────────────────────────────────────
// Payload of `code{...}` / `m{...}`. Balanced braces, depth-counted.
// `\{` `\}` `\\` never change depth. For `code` they unescape, for `math`
// they are preserved (KaTeX) — either way the scanner keeps raw bytes and
// the Rust compiler decodes (lexer.rs:436-477).
static bool scan_balanced(TSLexer *lexer) {
  int depth = 1; // outer `{` already consumed by the grammar
  bool consumed_any = false;
  for (;;) {
    int32_t c = lexer->lookahead;
    if (c == 0) {
      return false; // unterminated
    }
    if (c == '\\') {
      lexer->advance(lexer, false);
      int32_t n = lexer->lookahead;
      if (n == '{' || n == '}' || n == '\\') {
        lexer->advance(lexer, false);
        lexer->mark_end(lexer);
        consumed_any = true;
        continue;
      }
      lexer->mark_end(lexer);
      consumed_any = true;
      continue;
    }
    if (c == '{') {
      depth++;
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      consumed_any = true;
      continue;
    }
    if (c == '}') {
      depth--;
      if (depth == 0) {
        // Outer close: end BEFORE it (grammar consumes `}`).
        if (!consumed_any) {
          return false; // `code{}` / `m{}` — empty
        }
        return true;
      }
      lexer->advance(lexer, false);
      lexer->mark_end(lexer);
      consumed_any = true;
      continue;
    }
    lexer->advance(lexer, false);
    lexer->mark_end(lexer);
    consumed_any = true;
  }
}

// ── entry point ──────────────────────────────────────────────────────────────

bool tree_sitter_docup_external_scanner_scan(void *payload, TSLexer *lexer,
                                            const bool *valid_symbols) {
  (void)payload;
  // IMPORTANT: tree-sitter identifies the produced token via
  // `lexer->result_symbol` (external-token index, mapped through
  // ts_external_scanner_symbol_map in parser.c). Forgetting this reports
  // every token as index 0 (`_prose_text`) no matter what was scanned.
  if (valid_symbols[PROSE_TEXT] && scan_text(lexer, TEXT_PLAIN)) {
    lexer->result_symbol = PROSE_TEXT;
    return true;
  }
  if (valid_symbols[ITEM_TEXT] && scan_text(lexer, TEXT_ITEM)) {
    lexer->result_symbol = ITEM_TEXT;
    return true;
  }
  if (valid_symbols[QUOTE_TEXT] && scan_text(lexer, TEXT_QUOTE)) {
    lexer->result_symbol = QUOTE_TEXT;
    return true;
  }
  if (valid_symbols[RAW_CONTENT] && scan_raw_content(lexer)) {
    lexer->result_symbol = RAW_CONTENT;
    return true;
  }
  if (valid_symbols[CODE_CONTENT] && scan_balanced(lexer)) {
    lexer->result_symbol = CODE_CONTENT;
    return true;
  }
  if (valid_symbols[MATH_CONTENT] && scan_balanced(lexer)) {
    lexer->result_symbol = MATH_CONTENT;
    return true;
  }
  return false;
}
