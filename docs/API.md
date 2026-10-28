# API and MCP server

`rc_api_server` exposes the deterministic part of RealityCheck (the simulators, the declared-rule checker, the shrinker and
the regression-bundle writer) to other programs. **No model is called and no API key is needed**; it costs nothing to run.
The model-driven agent is separate (`rc_agent`, `rc_demo`).

    ./build/rc_api_server --port 8090 [--token SECRET]       # HTTP, listens on 127.0.0.1 only
    ./build/rc_api_server --mcp                               # Model Context Protocol over stdin/stdout

| HTTP | MCP tool | What it does |
|---|---|---|
| `GET /v1/domains` | `rc_domains` | domains, declared rules, scenario limits |
| `POST /v1/random_scenario` | `rc_random_scenario` | a valid random scenario `{domain, seed}` |
| `POST /v1/run` | `rc_run` | simulate and check every rule: violations, rule margins, trace hash (`include_signals` adds series) |
| `POST /v1/shrink` | `rc_shrink` | minimal scenario that still violates the same rule |
| `POST /v1/bundle` | `rc_bundle` | shrink, then return `scenario.json`, `expected.json`, `test.sh`, `README.md` |
| `POST /v1/hunt` | `rc_hunt` | random-testing hunt within a step budget |

Arguments: `domain` (`rigid2d`, `orbit`, `cartpole`, `traffic`), `engine` (`builtin` = clean reference, the default; `builtin:b02,b05`
= with those seeded bugs, for testing the tester; `box2d`, `box2d-noccd` when built with Box2D), `scenario` (an object of the domain).
The schema is in `docs/openapi.yaml`.

    curl -s localhost:8090/v1/hunt -d '{"domain":"orbit","engine":"builtin:o02","seed":3}'
    curl -s localhost:8090/v1/run  -d '{"domain":"orbit","engine":"builtin:o02","scenario":{...}}'

MCP client configuration (for example a client that reads an `mcpServers` map):

    {"mcpServers": {"realitycheck": {"command": "/path/to/build/rc_api_server", "args": ["--mcp"]}}}

## Safety

- Listens on loopback only. `--token` requires `Authorization: Bearer ...`. Request bodies are capped at 1 MB; scenarios are validated
  against the public limits before anything is simulated.
- `exec:<command>` engines run an arbitrary program, which would be remote code execution on a networked server. They are **refused
  by default** and enabled only with `--allow-exec`, which you should use only locally, with your own simulator.
- There is no ground truth in any response about which seeded bugs a *tester* should find; naming bugs in `engine` is an explicit
  operator choice for testing a tester.
- Not hosted anywhere: a public instance would need rate limits and a sandbox. This is a local service.

Tested in `rc_tests` (`api`) and by hand over HTTP (token, 401, exec refusal) and over MCP (initialize, tools/list, tools/call).
