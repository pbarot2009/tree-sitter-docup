//! Rust bindings for tree-sitter-docup.
//!
//! ```rust
//! let language = tree_sitter_docup::language();
//! let mut parser = tree_sitter::Parser::new();
//! parser.set_language(&language).unwrap();
//! let tree = parser.parse("p { Hello b{DocUP} }", None).unwrap();
//! ```

use tree_sitter_language::LanguageFn;

extern "C" {
    fn tree_sitter_docup() -> *const ();
}

/// Get the tree-sitter language for DocUP (.du).
pub fn language() -> tree_sitter::Language {
    unsafe { LanguageFn::from_raw(tree_sitter_docup).into() }
}

/// Recommended language constant for `tree-sitter` crate consumers.
pub const LANGUAGE: LanguageFn =
    unsafe { LanguageFn::from_raw(tree_sitter_docup) };

#[cfg(test)]
mod tests {
    use super::language;

    #[test]
    fn parses_paragraph_with_inline() {
        let mut parser = tree_sitter::Parser::new();
        parser.set_language(&language()).unwrap();
        let tree = parser.parse("p { Hello b{DocUP} }", None).unwrap();
        assert!(!tree.root_node().has_error());
        assert_eq!(tree.root_node().kind(), "document");
    }

    #[test]
    fn parses_verbatim_codeblock() {
        let mut parser = tree_sitter::Parser::new();
        parser.set_language(&language()).unwrap();
        let src = "codeblock(lang: \"go\") {!\nfmt.Println(\"}\")\n!}";
        let tree = parser.parse(src, None).unwrap();
        assert!(
            !tree.root_node().has_error(),
            "tree had error: {}",
            tree.root_node().to_sexp()
        );
    }
}
