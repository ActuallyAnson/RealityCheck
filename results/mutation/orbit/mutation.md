| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 136: operator '/' -> '*' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m001 | line 82: constant 1 -> 2 | unchanged | 0 | 0/6 | 0 |
| m002 | line 136: operator '/' -> '*' | killed_by_rules | 10 | 5/6 | 0 |
| m003 | line 134: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m004 | line 183: constant 1 -> 2 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m005 | line 102: constant 1.5 -> 1.5 * 1.1 | killed_by_rules | 918 | 6/6 | 0 |
| m006 | line 133: constant 12 -> 13 | unchanged | 0 | 0/6 | 0 |
| m007 | line 138: operator '==' -> '!=' | killed_by_rules | 243 | 6/6 | 0 |
| m008 | line 177: operator '+=' -> '-=' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m009 | line 110: operator '*' -> '/' | killed_by_rules | 948 | 6/6 | 994 |
| m010 | line 123: operator '*' -> '/' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m011 | line 123: operator '+=' -> '-=' | killed_by_rules | 951 | 6/6 | 989 |
| m012 | line 123: operator '*' -> '/' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m013 | line 133: operator '>' -> '>=' | unchanged | 0 | 0/6 | 0 |
| m014 | line 173: operator '+' -> '-' | killed_by_rules | 983 | 6/6 | 0 |
| m015 | line 123: constant 0 -> 1 | killed_by_rules | 994 | 6/6 | 993 |
| m016 | line 120: operator '<' -> '<=' | unchanged | 0 | 0/6 | 0 |
| m017 | line 99: operator '==' -> '!=' | killed_by_rules | 963 | 6/6 | 0 |
| m018 | line 177: operator '*' -> '/' | killed_by_rules | 742 | 6/6 | 820 |
| m019 | line 175: operator '*' -> '/' | killed_by_rules | 971 | 6/6 | 971 |
| m020 | line 183: operator '<=' -> '<' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m021 | line 120: operator '*' -> '/' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m022 | line 95: operator '*' -> '/' | killed_by_rules | 615 | 6/6 | 0 |
| m023 | line 97: operator '<' -> '<=' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m024 | line 101: operator '*' -> '/' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m025 | line 121: operator '*' -> '/' | crashed_or_timed_out | 0 | 0/6 | 0 |
| m026 | line 121: operator '+=' -> '-=' | killed_by_rules | 1000 | 6/6 | 1000 |
| m027 | line 98: operator '<' -> '<=' | unchanged | 0 | 0/6 | 0 |
| m028 | line 98: constant 0 -> 1 | killed_by_rules | 999 | 6/6 | 999 |
| m029 | line 120: constant 0.5 -> 0.5 * 1.1 | killed_by_rules | 202 | 6/6 | 821 |
| m030 | line 123: constant 0.5 -> 0.5 * 1.1 | killed_by_rules | 226 | 6/6 | 821 |
| m031 | line 182: constant 1 -> 2 | unchanged | 0 | 0/6 | 0 |
| m032 | line 120: operator '+=' -> '-=' | killed_by_rules | 950 | 6/6 | 989 |
| m033 | line 97: constant 0 -> 1 | killed_by_rules | 999 | 6/6 | 999 |
| m034 | line 120: operator '*' -> '/' | killed_by_rules | 1000 | 6/6 | 1000 |
| m035 | line 101: operator '+' -> '-' | killed_by_rules | 983 | 6/6 | 938 |
| m036 | line 138: constant 1 -> 2 | unchanged | 0 | 0/6 | 0 |
| m037 | line 85: constant 0 -> 1 | unchanged | 0 | 0/6 | 0 |
| m038 | line 102: operator '+' -> '-' | killed_by_rules | 581 | 6/6 | 518 |
| m039 | line 133: operator '&&' -> '||' | crashed_or_timed_out | 0 | 0/6 | 0 |

orbit: 40 mutants; clean pool of 1000 scenarios had 0 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 5
- crashed_or_timed_out: 7
- killed_by_rules: 20
- unchanged: 8
Among mutants whose traces differ from the clean simulator on the pool (25): killed by the hand-written rules 20, by the model-written invariants 14, by either 20.
Metamorphic oracles (first 300 scenarios of the pool): killed 8 of the changed mutants; hand-written rules or metamorphic: 20; metamorphic only (no declared rule fires): 0; all three oracles: 20.
Killed mutants by number of step-budget windows in which random testing finds them (of 6): 5 windows: 1; 6 windows: 19; 
