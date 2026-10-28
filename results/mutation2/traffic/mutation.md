| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 122: constant 0 -> 1 + operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m001 | line 122: constant 0 -> 1 + constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m002 | line 167: operator '+' -> '-' + operator '==' -> '!=' | killed_by_rules | 397 | 5/5 | 0 |
| m003 | line 147: constant 2 -> 3 + constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m004 | line 191: operator '&&' -> '||' + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m005 | line 131: operator '||' -> '&&' + constant 4 -> 5 | killed_by_rules | 999 | 5/5 | 0 |
| m006 | line 147: operator '<' -> '<=' + operator '==' -> '!=' | killed_by_rules | 1000 | 5/5 | 0 |
| m007 | line 248: constant 0 -> 1 + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m008 | line 238: operator '<' -> '<=' + operator '+=' -> '-=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m009 | line 120: constant 0 -> 1 + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m010 | line 244: constant 1 -> 2 + operator '>' -> '>=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m011 | line 229: operator '&&' -> '||' + operator '==' -> '!=' | killed_by_rules | 924 | 5/5 | 0 |
| m012 | line 245: operator '!=' -> '==' + constant 4 -> 5 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m013 | line 237: operator '<' -> '<=' + operator '>=' -> '>' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m014 | line 235: constant 0 -> 1 + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m015 | line 137: operator '>=' -> '>' + operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m016 | line 241: constant 4 -> 5 + operator '<' -> '<=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m017 | line 165: operator '||' -> '&&' + constant 0 -> 1 | killed_by_rules | 1000 | 5/5 | 0 |
| m018 | line 157: operator '==' -> '!=' + constant 0 -> 1 | killed_by_rules | 1000 | 5/5 | 0 |
| m019 | line 242: operator '>' -> '>=' + operator '>=' -> '>' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m020 | line 165: operator '==' -> '!=' + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m021 | line 142: operator '==' -> '!=' + operator '||' -> '&&' | killed_by_rules | 924 | 5/5 | 0 |
| m022 | line 188: operator '>=' -> '>' + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m023 | line 244: operator '>=' -> '>' + constant 4 -> 5 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m024 | line 163: constant 0 -> 1 + constant 0 -> 1 | killed_by_rules | 1000 | 5/5 | 0 |
| m025 | line 211: constant 0 -> 1 + constant 200 -> 201 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m026 | line 180: operator '==' -> '!=' + operator '==' -> '!=' | killed_by_rules | 1000 | 5/5 | 0 |
| m027 | line 131: operator '||' -> '&&' + operator '-' -> '+' | killed_by_rules | 924 | 5/5 | 0 |
| m028 | line 122: constant 0 -> 1 + operator '==' -> '!=' | killed_by_rules | 957 | 5/5 | 0 |
| m029 | line 191: operator '>=' -> '>' + constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |

traffic: 30 mutants; clean pool of 1000 scenarios had 0 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 19
- killed_by_rules: 11
Among mutants whose traces differ from the clean simulator on the pool (30): killed by the hand-written rules 11, by the model-written invariants 0, by either 11.
Metamorphic oracles (first 300 scenarios of the pool): killed 0 of the changed mutants; hand-written rules or metamorphic: 11; metamorphic only (no declared rule fires): 0; all three oracles: 11.
Killed mutants by number of step-budget windows in which random testing finds them (of 5): 5 windows: 11; 
