# Guided search: tested offline first, and it did not work

Idea: give the tester a deterministic "margin" signal (how close each rule came to failing) and let the engine hill-climb
around the model's scenarios, so the model chooses where to start and code does the climbing. Before building any model-facing
part I tested the premise with no model at all (`rc_climb`, free):

- `rule_margins(scenario, trace)` (`src/rules/checker.cpp`): per continuous rule, the maximum over steps of measured / allowed;
  1 or more means violated. Rules with no continuous quantity (NaN, tunneling, conflicts, signal durations) have no margin.
- A hill-climber: start from a random scenario, mutate one numeric field at a time (within the public limits; invalid
  mutations are dropped for free), keep the mutation if the largest margin does not drop, restart after 25 mutations.
  Every simulation, including mutations, is charged against the same 300000-step budget as random sampling.
- 10 seeds per bug; a hit counts only if the violation does not also appear on the clean simulator for the same scenario.

Result (`results/climb/offline.md`, reproduced by the commands in that file's header): **random sampling finds the bug at
least as often as the hill-climber in 14 of 15 bug rows, and usually needs fewer simulations.** For example b11 9/10 against
4/10, c01 10/10 against 5/10, o01 10/10 against 3/10. The one exception is c03, where the climber needed fewer simulations
(19 against 41) but found it in 3/10 seeds against 10/10. The bugs that random sampling cannot find (b02, b12, c02) were not
found by the climber either.

Why, as far as the data says: most seeded bugs fire on a discrete condition (a body count, a step size, a duration) and the
rule margin is flat or uninformative until the bug fires, so there is no gradient to follow; and the mutations spend budget on
long scenarios near a poor start.

Consequence: I did not build the model-facing version (margin field in tool output, a `local_search` tool, agent v3), because
without a working no-model climber there is nothing for the model to add, and the claim would have been untestable. This is the
control I would have needed anyway. Not done, so not claimed: a better-designed climber (structural mutations, margins
for duration rules, adaptive restarts) might do better; this one was not tuned after seeing the results.
