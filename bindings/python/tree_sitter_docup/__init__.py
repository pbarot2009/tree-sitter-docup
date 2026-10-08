"""Python bindings for tree-sitter-docup."""

from tree_sitter import Language
import tree_sitter_docup as _native


def language() -> Language:
    return Language(_native.language())
