#!/usr/bin/env bash
# Fail-closed check: every tracked text file must use LF in the index and work tree.
# Binary entries and gitlinks are ignored via git's text/eol attributes.
#
# 运行：bash scripts/check_line_endings.sh
# 退出码：0 = 全部 LF；1 = 存在 CRLF/mixed 或不在 Git 工作树。

set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  printf '[line-endings] FAIL: not inside a git work tree\n' >&2
  exit 1
fi

bad="$(
  git ls-files --eol |
    awk -F'\t' '
      {
        split($1, eol, " ");
        if (eol[1] ~ /^i\/(crlf|mixed)$/ || eol[2] ~ /^w\/(crlf|mixed)$/) {
          print $2;
        }
      }
    '
)"

if [[ -n "${bad}" ]]; then
  printf '[line-endings] FAIL: CRLF or mixed line endings detected in tracked text files:\n' >&2
  printf '%s\n' "${bad}" | sed 's/^/  - /' >&2
  printf '[line-endings] normalize with: git add --renormalize . && git checkout -- .\n' >&2
  exit 1
fi

printf '[line-endings] OK: all tracked text files use LF\n'
