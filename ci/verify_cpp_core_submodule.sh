#!/usr/bin/env sh
set -eu

submodule_path="${CPP_CORE_SUBMODULE_PATH:-third_party/cpp-core}"
tag_pattern="${CPP_CORE_TAG_PATTERN:-v[0-9]*.[0-9]*.[0-9]*}"

if ! git -C "$submodule_path" rev-parse --verify HEAD >/dev/null 2>&1; then
  echo "cpp-core submodule is missing or not initialized: $submodule_path"
  exit 1
fi

tag="$(git -C "$submodule_path" tag --points-at HEAD | head -n 1 || true)"

if [ -z "$tag" ]; then
  echo "cpp-core submodule is not pinned to a release tag"
  exit 1
fi

case "$tag" in
  $tag_pattern)
    echo "cpp-core pinned to release tag: $tag"
    ;;
  *)
    echo "cpp-core tag is not semver-shaped: $tag"
    exit 1
    ;;
esac
