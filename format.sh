find llvm/lib/Target/ETCA clang/lib/Driver/ToolChains/ETCA.* -type f -not -path './.git/*' | git check-ignore -nv --stdin | rg '^::\t(.*)$' -r '$1' --color=never | rg '\.(cpp|c|cc|h|hpp|hxx|cpp|td)$' | xargs -I{} clang-format -i {}

