#!/usr/bin/env bash
# AI-generated because the toolchains suck and I shouldn't have to deal with it
D="$(dirname "$0")"

if [[ "$(uname)" == "Darwin" ]]; then
    clang -o "$D/_asmrun_out" "$1" || exit 1
    shift

    BP_ARGS=()
    for bp in "$@"; do
        BP_ARGS+=(-o "breakpoint set --name $bp")
    done

    lldb --no-lldbinit \
        -o "target create $D/_asmrun_out" \
        "${BP_ARGS[@]}" \
        -o "process launch"

else
    # x86_64 Linux: cross-compile for aarch64, run under qemu
    # Requires: aarch64-linux-gnu-gcc, qemu-aarch64-static
    # For breakpoints also requires: aarch64-linux-gnu-gdb, qemu-user (non-static)

    # Translate macOS Mach-O conventions to ELF/Linux conventions
    TMP="$(mktemp --suffix=.S)"
    sed \
        -e 's/\.section __TEXT,__text/.section .text/g' \
        -e 's/\.section __DATA,__data/.section .data/g' \
        -e 's/\b_main\b/main/g' \
        "$1" > "$TMP"

    aarch64-linux-gnu-gcc -static -o "$D/_asmrun_out" "$TMP"; RC=$?
    rm -f "$TMP"
    [[ $RC -eq 0 ]] || exit 1
    shift

    BP_ARGS=()
    for bp in "$@"; do
        BP_ARGS+=(-ex "break $bp")
    done

    qemu-aarch64-static -g 54321 "$D/_asmrun_out" &
    QEMU_PID=$!

    aarch64-linux-gnu-gdb \
        -ex "set debuginfod enabled off" \
        -ex "target remote :54321" \
        "${BP_ARGS[@]}" \
        -ex "continue" \
        "$D/_asmrun_out"

    kill "$QEMU_PID" 2>/dev/null; wait "$QEMU_PID" 2>/dev/null || true
fi
