| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 114: operator '*' -> '/' + operator '*' -> '/' | killed_by_rules | 956 | 7/7 | 930 |
| m001 | line 178: operator '+' -> '-' + operator '-' -> '+' | killed_by_rules | 1000 | 7/7 | 1000 |
| m002 | line 188: operator '+=' -> '-=' + constant 1 -> 2 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m003 | line 114: operator '*' -> '/' + operator '<' -> '<=' | killed_by_rules | 977 | 7/7 | 926 |
| m004 | line 171: constant 0.0 -> 0.0 * 1.1 + operator '*' -> '/' | unchanged | 0 | 0/7 | 0 |
| m005 | line 113: operator '/' -> '*' + operator '*' -> '/' | killed_by_rules | 1000 | 7/7 | 1000 |
| m006 | line 114: operator '+' -> '-' + operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m007 | line 182: constant 2.5 -> 2.5 * 1.1 + operator '<' -> '<=' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m008 | line 109: operator '+' -> '-' + operator '/' -> '*' | killed_by_rules | 668 | 7/7 | 668 |
| m009 | line 111: operator '*' -> '/' + operator '+=' -> '-=' | killed_by_rules | 1000 | 7/7 | 1000 |
| m010 | line 103: operator '*' -> '/' + operator '+' -> '-' | killed_by_rules | 1000 | 7/7 | 1000 |
| m011 | line 115: operator '/' -> '*' + operator '*' -> '/' | killed_by_rules | 984 | 7/7 | 694 |
| m012 | line 171: constant 0 -> 1 + operator '+=' -> '-=' | killed_by_rules | 963 | 7/7 | 928 |
| m013 | line 128: operator '<' -> '<=' + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m014 | line 134: operator '+' -> '-' + operator '*' -> '/' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m015 | line 109: operator '/' -> '*' + operator '/' -> '*' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m016 | line 164: constant 0.005 -> 0.005 * 1.1 + operator '*' -> '/' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m017 | line 191: operator '+' -> '-' + operator '+' -> '-' | killed_by_rules | 460 | 7/7 | 0 |
| m018 | line 190: operator '+' -> '-' + operator '*' -> '/' | killed_by_rules | 984 | 7/7 | 975 |
| m019 | line 111: operator '+' -> '-' + constant 1 -> 2 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m020 | line 124: constant 0 -> 1 + operator '*' -> '/' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m021 | line 134: operator '+' -> '-' + constant 4.0 -> 4.0 * 1.1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m022 | line 103: operator '*' -> '/' + constant 4 -> 5 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m023 | line 110: operator '*' -> '/' + operator '+' -> '-' | killed_by_rules | 40 | 7/7 | 30 |
| m024 | line 178: operator '*' -> '/' + constant 4 -> 5 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m025 | line 103: operator '*' -> '/' + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m026 | line 109: constant 2 -> 3 + operator '*' -> '/' | killed_by_rules | 1000 | 7/7 | 381 |
| m027 | line 126: operator '*' -> '/' + operator '*' -> '/' | killed_by_rules | 1000 | 7/7 | 914 |
| m028 | line 126: constant 1 -> 2 + constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m029 | line 125: operator '+=' -> '-=' + operator '>' -> '>=' | killed_by_rules | 1000 | 7/7 | 1000 |

cartpole: 30 mutants; clean pool of 1000 scenarios had 0 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 14
- killed_by_rules: 15
- unchanged: 1
Among mutants whose traces differ from the clean simulator on the pool (29): killed by the hand-written rules 15, by the model-written invariants 14, by either 15.
Metamorphic oracles (first 300 scenarios of the pool): killed 11 of the changed mutants; hand-written rules or metamorphic: 15; metamorphic only (no declared rule fires): 0; all three oracles: 15.
Killed mutants by number of step-budget windows in which random testing finds them (of 7): 7 windows: 15; 
