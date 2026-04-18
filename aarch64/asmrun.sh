D="$(dirname "$0")"
clang -o "$D/_asmrun_out" "$1" || exit 1
shift

BP_ARGS=()
for bp in "$@"; do
    BP_ARGS+=(-o "breakpoint set --name $bp")
done

lldb --no-lldbinit --batch \
    -o "target create $D/_asmrun_out" \
    "${BP_ARGS[@]}" \
    -o "process launch"

