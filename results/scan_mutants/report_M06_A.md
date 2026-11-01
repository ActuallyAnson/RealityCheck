# Scan of M06_A

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 13 candidate invariants.

No finding. This is not a certificate: a bug present in every run looks normal to learned invariants (docs/MINING.md). Add your own rules to `card.json` (docs/GENERIC_DOMAIN.md) or review the learned invariants.

Re-run with the saved card: `RC_MODEL_CARD=/private/tmp/claude-501/-Users-anson-Documents-GitHub-RealityCheck/1b9430b7-3ec4-4ee5-83ff-494a23359799/scratchpad/agents/mutants/scans/M06_A/card.json rc_hunt --engine card --domain generic --scenarios 1000`; add `rc_agent --live` for a model-driven hunt or `rc_spec --live` for model-written rules.
Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
