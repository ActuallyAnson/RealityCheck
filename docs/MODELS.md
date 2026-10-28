# Using other models

The model client speaks the OpenAI-style Chat Completions protocol with tool calls (and image inputs for frames), shelling out to `curl`. Any endpoint that does is usable by pointing `--config` at a JSON file:

| config | endpoint | notes |
|---|---|---|
| `configs/hosted.json` | Meta Model API, `muse-spark-1.3` (default) | key in `MODEL_API_KEY` |
| `configs/hosted_1_2.json`, `hosted_1_1.json` | same API, older models | used for the portability check in `docs/TRIAGE.md` |
| `configs/open_weight.json` | Baseten-hosted open weights | untested: needs a `BASETEN_API_KEY` and credits |
| `configs/local_openai_compat.json` | a local llama.cpp, vLLM or Ollama server on 127.0.0.1 | no key (`"api_key_env": ""`), zero price, `native_tools: false` (the strict JSON-action protocol) |

Set `native_tools` to `false` when a server does not implement tool calls: the agent then uses a JSON action protocol and everything else (tools, validation, budgets) is unchanged. Prices in the config feed the per-run budget and the lifetime spend ledger.

Status, stated plainly: the keyless path and the base-URL override were checked against a stub server (request reached the configured URL, no `Authorization` header sent, model name passed through). **No local or open-weight model has been run through the agent**, so there is no result for one. Running a local 7B to 70B model also loads
the machine heavily; check your hardware before trying.
