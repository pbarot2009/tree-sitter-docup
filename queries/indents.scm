; Indent rules for editors with tree-sitter indent (Helix/Zed)
[
  (meta_block)
  (heading)
  (paragraph)
  (codeblock)
  (list_block)
  (list_item)
  (task_item)
  (quote_block)
  (table_block)
  (table_row)
  (table_cell)
  (callout_block)
  (footnote_def)
] @indent.begin
"}" @indent.end @indent.branch
"!}" @indent.end @indent.branch
