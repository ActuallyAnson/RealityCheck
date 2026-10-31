# Scan of rebound-random

600 scenarios (half pushed to parameter boundaries), 200 normal runs mined, 9 candidate invariants.

No finding. This is not a certificate: a bug present in every run looks normal to learned invariants (docs/MINING.md). Add your own rules to `card.json` (docs/GENERIC_DOMAIN.md) or review the learned invariants.

Learned invariants are candidates, not specifications: one that fires may be a bug or an unusual but valid input; only you can tell.
