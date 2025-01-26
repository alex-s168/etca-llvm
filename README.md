# LLVM For ETC.A
This is currently work in progress. Use at your own risk.

## Compiling LLVM 
To compile LLVM with only the ETC.A backend:
```shell
cmake -B build -S llvm -G Ninja -DLLVM_ENABLE_PROJECTS=llvm -DLLVM_PARALLEL_{COMPILE,LINK}_JOBS=$(nproc) -DLLVM_TARGETS_TO_BUILD="" -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="Etca" -DCMAKE_BUILD_TYPE=Debug
ninja -C build -j$(nproc)
```

## Testing 
To compile the LLVM IR file `in.ll`, simply run this command to generate a assembly file (`out.s`)
```shell 
build/bin/llc --mtriple etca -filetype=asm -o out.s < in.ll
```
