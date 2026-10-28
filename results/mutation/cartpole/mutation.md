| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 127: operator '*' -> '/' | killed_by_rules | 506 | 7/7 | 354 |
| m001 | line 110: operator '*' -> '/' | killed_by_rules | 40 | 7/7 | 30 |
| m002 | line 171: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m003 | line 101: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m004 | line 182: constant 2.5 -> 2.5 * 1.1 | unchanged | 0 | 0/7 | 0 |
| m005 | line 112: operator '+=' -> '-=' | killed_by_rules | 895 | 7/7 | 769 |
| m006 | line 110: constant 2 -> 3 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m007 | line 109: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m008 | line 103: operator '*' -> '/' | killed_by_rules | 1000 | 7/7 | 1000 |
| m009 | line 187: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m010 | line 101: operator '*' -> '/' | killed_by_rules | 1000 | 7/7 | 384 |
| m011 | line 114: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m012 | line 112: constant 6 -> 7 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m013 | line 127: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m014 | line 110: constant 2 -> 3 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m015 | line 178: operator '+' -> '-' | killed_by_rules | 952 | 7/7 | 881 |
| m016 | line 166: operator '/' -> '*' | unchanged | 0 | 0/7 | 0 |
| m017 | line 113: operator '+=' -> '-=' | killed_by_rules | 993 | 7/7 | 957 |
| m018 | line 126: constant 4 -> 5 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m019 | line 114: operator '*' -> '/' | killed_by_rules | 965 | 7/7 | 918 |
| m020 | line 188: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m021 | line 115: operator '+' -> '-' | killed_by_rules | 997 | 7/7 | 986 |
| m022 | line 182: operator '>' -> '>=' | unchanged | 0 | 0/7 | 0 |
| m023 | line 109: operator '*' -> '/' | unchanged | 0 | 0/7 | 0 |
| m024 | line 191: operator '+' -> '-' | killed_by_rules | 458 | 7/7 | 0 |
| m025 | line 115: operator '+=' -> '-=' | killed_by_rules | 1000 | 7/7 | 1000 |
| m026 | line 115: constant 2 -> 3 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m027 | line 110: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m028 | line 111: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m029 | line 124: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m030 | line 109: operator '+' -> '-' | unchanged | 0 | 0/7 | 0 |
| m031 | line 111: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m032 | line 124: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/7 | 0 |
| m033 | line 127: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m034 | line 169: operator '-' -> '+' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m035 | line 134: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/7 | 0 |
| m036 | line 113: constant 2 -> 3 | killed_by_rules | 57 | 7/7 | 0 |
| m037 | line 103: operator '+' -> '-' | killed_by_rules | 13 | 7/7 | 0 |
| m038 | line 103: operator '*' -> '/' | killed_by_rules | 150 | 7/7 | 147 |
| m039 | line 134: operator '+' -> '-' | killed_by_rules | 156 | 7/7 | 153 |

cartpole: 40 mutants; clean pool of 1000 scenarios had 0 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 20
- killed_by_rules: 15
- unchanged: 5
Among mutants whose traces differ from the clean simulator on the pool (35): killed by the hand-written rules 15, by the model-written invariants 12, by either 15.
Killed mutants by number of step-budget windows in which random testing finds them (of 7): 7 windows: 15; 
