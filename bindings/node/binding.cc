#include <node_api.h>
#include <tree_sitter/api.h>

// Declared by the generated parser + our scanner.
extern "C" const TSLanguage *tree_sitter_docup(void);

static napi_value GetLanguage(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_create_external(
      env, (void *)tree_sitter_docup(), nullptr, nullptr, &result);
  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc = {"language", nullptr, nullptr, GetLanguage,
                                   nullptr,         nullptr, napi_default,
                                   nullptr};
  napi_define_properties(env, exports, 1, &desc);
  return exports;
}

NAPI_MODULE(tree_sitter_docup_binding, Init)
