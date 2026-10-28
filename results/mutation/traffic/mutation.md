| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 122: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m001 | line 238: operator '<' -> '<=' | unchanged | 0 | 0/5 | 0 |
| m002 | line 244: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m003 | line 179: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m004 | line 238: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m005 | line 142: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m006 | line 122: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m007 | line 236: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m008 | line 122: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m009 | line 189: operator '>' -> '>=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m010 | line 248: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m011 | line 237: constant 4 -> 5 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m012 | line 142: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m013 | line 195: constant 0 -> 1 | killed_by_rules | 1000 | 5/5 | 0 |
| m014 | line 122: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m015 | line 165: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m016 | line 210: constant 0 -> 1 | killed_by_rules | 924 | 5/5 | 0 |
| m017 | line 242: constant 2 -> 3 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m018 | line 158: constant 2 -> 3 | killed_by_rules | 1000 | 5/5 | 1000 |
| m019 | line 137: operator '&&' -> '||' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m020 | line 158: operator '==' -> '!=' | killed_by_rules | 1000 | 5/5 | 1000 |
| m021 | line 180: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m022 | line 157: constant 0 -> 1 | killed_by_rules | 1000 | 5/5 | 0 |
| m023 | line 188: operator '>=' -> '>' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m024 | line 137: operator '>=' -> '>' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m025 | line 183: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m026 | line 176: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m027 | line 176: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m028 | line 242: operator '>' -> '>=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m029 | line 180: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m030 | line 142: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m031 | line 142: operator '&&' -> '||' | killed_by_rules | 531 | 5/5 | 0 |
| m032 | line 142: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m033 | line 137: operator '==' -> '!=' | changed_no_rule_fires | 0 | 0/5 | 0 |
| m034 | line 131: operator '||' -> '&&' | killed_by_rules | 999 | 5/5 | 0 |
| m035 | line 191: operator '&&' -> '||' | unchanged | 0 | 0/5 | 0 |
| m036 | line 142: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m037 | line 229: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m038 | line 120: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/5 | 0 |
| m039 | line 191: operator '||' -> '&&' | changed_no_rule_fires | 0 | 0/5 | 0 |

traffic: 40 mutants; clean pool of 1000 scenarios had 0 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 31
- killed_by_rules: 7
- unchanged: 2
Among mutants whose traces differ from the clean simulator on the pool (38): killed by the hand-written rules 7, by the model-written invariants 2, by either 7.
Killed mutants by number of step-budget windows in which random testing finds them (of 5): 5 windows: 7; 
