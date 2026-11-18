#!/bin/sh
# build.sh LIST ROOT - materialize the paths in LIST into ROOT exactly as
# they exist on the source system.
# Pass 0 creates every parent directory under its canonical (symlink-free)
# path, pass 1 lays down the listed symlinks themselves (a path always
# sorts before anything beneath it), pass 2 copies everything else.
R=$2
while IFS= read -r p; do
    c=$(realpath "$(dirname "$p")" 2>/dev/null) || continue
    mkdir -p "$R$c"
done < "$1"
while IFS= read -r p; do
    if [ -L "$p" ]; then ln -sfn "$(readlink "$p")" "$R$p"; fi
done < "$1"
while IFS= read -r p; do
    if [ -L "$p" ]; then continue; fi
    cp -a "$p" "$R$p" || echo "build.sh: could not copy $p" >&2
done < "$1"
