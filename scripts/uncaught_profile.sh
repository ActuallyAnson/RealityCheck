#!/bin/sh
# Profiles the changed mutants that no oracle catches (docs/MUTATION.md, docs/REFERENCE_ORACLE.md): for each, the share of the 1000-scenario pool whose
# behavior changed at all, and the source edit. A mutant that changes behavior in most scenarios is not "rare"; one that changes it in a handful may be
# equivalent for practical purposes. This script cannot say which are harmless; it says how visible each change is. No model. Needs build_dev or build.
cd "$(dirname "$0")/.."
B=${BUILD_DIR:-build}
printf "domain\tid\tscenarios_changed/1000\tedit\n"
for d in orbit cartpole traffic rigid2d; do
  J=results/mutation/$d/mutation.json
  if [ $d = rigid2d ]; then
    jq -r '.mutants[]|select(.class=="changed_no_rule_fires" and .invariants_fire_in_scenarios==0 and .metamorphic_fire_in_scenarios==0)|.id' $J > /tmp/_unc.$$
  else
    R=results/reforacle/$d/mutants; [ $d = traffic ] && R=results/reforacle/traffic/draw2/mutants
    "$B/rc_refscore" mutants $J $R | awk -F'\t' '$2=="silent" && $5!="CAUGHT"{print $1}' > /tmp/_unc.$$
  fi
  while read -r id; do
    jq -r --arg id "$id" --arg d "$d" '.mutants[]|select(.id==$id)|[$d,.id,(.scenarios_changed|tostring),.change+" (line "+(.line|tostring)+")"]|@tsv' $J
  done < /tmp/_unc.$$
  rm -f /tmp/_unc.$$
done
