#!/bin/sh
# Bundles one recorded run into a single self-contained HTML file (viewer, playback, evidence, no server, no network).
# usage: scripts/share_report.sh <run_id> [out.html]      e.g. scripts/share_report.sh t01_5101 report_t01.html
set -e
cd "$(dirname "$0")/.."
id="$1"; out="${2:-report_$1.html}"
[ -n "$id" ] && [ -f "web/data/$id.json" ] || { echo "no such run: web/data/$id.json (run ./build/rc_eval export first)" >&2; exit 2; }
esc() { sed 's#</#<\\/#g' "$1"; }   # keep embedded JSON/JS from closing the script tag
{
  sed -n '1,/<link rel="stylesheet"/p' web/index.html | sed '$d'
  echo '<style>'; cat web/style.css; echo '.modebar,#runSel,#modeBadge,.field:has(#runSel),nav.seg{display:none!important}</style>'
  sed -n '/<link rel="stylesheet"/,$p' web/index.html | sed '1d' | sed '/<script src=/d'
} | sed '$d' | sed '$d' > "$out.head"
{
  cat "$out.head"
  echo '<script>'
  printf 'const __RC={index:'; printf '[{"id":"%s","bug":"%s","status":"failure_reported"}]' "$id" "${id%%_*}"; printf ',run:'; esc "web/data/$id.json"; echo '};'
  echo 'const __f=window.fetch;window.fetch=(u)=>{const j=(x)=>Promise.resolve({json:()=>Promise.resolve(x)});u=String(u);if(u.endsWith("data/index.json"))return j(__RC.index);if(u.endsWith("data/'"$id"'.json"))return j(__RC.run);return Promise.reject(new Error("offline report"));};'
  echo '</script>'
  echo '<script>'; esc web/audio.js; echo '</script>'
  echo '<script>'; esc web/app.js; echo '</script>'
  echo '</body></html>'
} > "$out"
rm -f "$out.head"
echo "wrote $out ($(wc -c < "$out") bytes)"
