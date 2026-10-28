| mutant | change | class | scenarios violating a hand-written rule | windows (step budget) where random testing finds it | model-written invariants fire |
|---|---|---|---|---|---|
| m000 | line 238: operator '*' -> '/' | unchanged | 0 | 0/6 | 0 |
| m001 | line 38: constant 1.0 -> 1.0 * 1.1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m002 | line 144: operator '!=' -> '==' | unchanged | 0 | 0/6 | 0 |
| m003 | line 83: operator '-' -> '+' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m004 | line 78: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m005 | line 228: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m006 | line 36: constant 0 -> 1 | crashed_or_timed_out | 0 | 0/6 | 0 |
| m007 | line 48: operator '*' -> '/' | killed_by_rules | 79 | 6/6 | 0 |
| m008 | line 237: operator '+=' -> '-=' | unchanged | 0 | 0/6 | 0 |
| m009 | line 241: operator '+=' -> '-=' | killed_by_rules | 346 | 6/6 | 3 |
| m010 | line 238: operator '*' -> '/' | unchanged | 0 | 0/6 | 0 |
| m011 | line 198: operator '<' -> '<=' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m012 | line 130: operator '*' -> '/' | killed_by_rules | 9 | 4/6 | 0 |
| m013 | line 140: operator '-=' -> '+=' | killed_by_rules | 516 | 6/6 | 464 |
| m014 | line 163: operator '+=' -> '-=' | unchanged | 0 | 0/6 | 0 |
| m015 | line 155: operator '*' -> '/' | killed_by_rules | 968 | 6/6 | 958 |
| m016 | line 78: operator '-' -> '+' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m017 | line 213: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/6 | 482 |
| m018 | line 184: operator '-' -> '+' | unchanged | 0 | 0/6 | 0 |
| m019 | line 174: operator '-' -> '+' | killed_by_rules | 633 | 6/6 | 0 |
| m020 | line 243: operator '||' -> '&&' | unchanged | 0 | 0/6 | 0 |
| m021 | line 103: operator '/' -> '*' | killed_by_rules | 436 | 6/6 | 434 |
| m022 | line 243: operator '*' -> '/' | killed_by_rules | 18 | 5/6 | 0 |
| m023 | line 48: operator '+' -> '-' | killed_by_rules | 709 | 6/6 | 1 |
| m024 | line 47: constant 0 -> 1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m025 | line 121: operator '*' -> '/' | unchanged | 0 | 0/6 | 0 |
| m026 | line 80: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m027 | line 80: constant 0.3 -> 0.3 * 1.1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m028 | line 38: constant 1.0 -> 1.0 * 1.1 | changed_no_rule_fires | 0 | 0/6 | 0 |
| m029 | line 160: operator '*' -> '/' | killed_by_rules | 966 | 6/6 | 963 |
| m030 | line 120: constant 0.8 -> 0.8 * 1.1 | unchanged | 0 | 0/6 | 0 |
| m031 | line 84: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m032 | line 119: operator '/' -> '*' | unchanged | 0 | 0/6 | 0 |
| m033 | line 131: operator '+' -> '-' | killed_by_rules | 216 | 6/6 | 42 |
| m034 | line 111: operator '+' -> '-' | changed_no_rule_fires | 0 | 0/6 | 0 |
| m035 | line 243: constant 0 -> 1 | unchanged | 0 | 0/6 | 0 |
| m036 | line 121: constant 0.5 -> 0.5 * 1.1 | unchanged | 0 | 0/6 | 0 |
| m037 | line 120: constant 0.8 -> 0.8 * 1.1 | unchanged | 0 | 0/6 | 0 |
| m038 | line 217: operator '<' -> '<=' | unchanged | 0 | 0/6 | 0 |
| m039 | line 145: operator '*' -> '/' | unchanged | 0 | 0/6 | 0 |

rigid2d: 40 mutants; clean pool of 1000 scenarios had 1 hand-rule and 0 invariant violations (both must be 0).
- changed_no_rule_fires: 13
- crashed_or_timed_out: 1
- killed_by_rules: 11
- unchanged: 15
Among mutants whose traces differ from the clean simulator on the pool (24): killed by the hand-written rules 11, by the model-written invariants 8, by either 12.
Metamorphic oracles (first 300 scenarios of the pool): killed 8 of the changed mutants; hand-written rules or metamorphic: 14; metamorphic only (no declared rule fires): 3; all three oracles: 15.
Killed mutants by number of step-budget windows in which random testing finds them (of 6): 4 windows: 1; 5 windows: 1; 6 windows: 9; 
