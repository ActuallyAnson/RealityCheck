# Generic-domain example: a water-tank controller

`tank.cpp` is a stand-alone program (it links only the JSON helper) that speaks the generic-domain protocol; `tank_card.json` is its model card. It has a **deliberate flaw**: in proportional
mode with gain above 4 and capacity of at least 40, the code that clamps the level at capacity and records the spill is skipped, so the level can exceed capacity. The flaw is mine and the example exists
to show the pipeline on a target that is not one of the built-in domains.

    cmake --build build && export RC_MODEL_CARD=examples/generic/tank_card.json
    ./build/rc_hunt --engine card --domain generic --scenarios 1500 --bundles out/        # random hunt, no model
    ./build/rc_agent --live --domain generic --engine card --agent-version 2 --out runs/x   # model-driven hunt
    ./build/rc_spec --domain generic --engine card --live --out runs/spec                   # model writes invariants from the card

Results: `docs/GENERIC_DOMAIN.md`.
