{
  "targets": [
    {
      "target_name": "tree_sitter_docup_binding",
      "sources": [
        "bindings/node/binding.cc",
        "src/parser.c",
        "src/scanner.c"
      ],
      "include_dirs": [
        "src"
      ],
      "cflags_c": ["-std=c11"]
    }
  ]
}
