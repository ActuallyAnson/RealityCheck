# Step 0: Meta Model API and Baseten, from public documentation only

Read on 2026-10-27 from dev.meta.ai (the old ai.developer.meta.com / developer.meta.com URLs redirect there) and
docs.baseten.co. No API call was made. Anything not stated in the docs is marked **not stated**.

## Models (https://dev.meta.ai/docs/models)
| Model | ID | Image in | Tools | Structured output | Context |
|---|---|---|---|---|---|
| Muse Spark 1.3 | `muse-spark-1.3` | yes | yes | yes | 1,048,576 |
| Muse Spark 1.2 | `muse-spark-1.2` | yes | yes | yes | 1,048,576 |
| Muse Spark 1.1 | `muse-spark-1.1` | yes | yes | yes | 1,048,576 |
| Muse Spark 1.3 / 1.2 Contributor | `muse-spark-1.3-contributor`, `muse-spark-1.2-contributor` | yes (family) | yes (family) | yes (family) | not stated |
| Muse Wonder (preview) | `muse-wonder-1.0` | yes | yes | yes | 1,048,576 |
| Muse Image / Voice Transcribe / SAM 3.1 | `muse-image-1.0`, `muse-voice-transcribe-1.0`, `sam-3.1` | n/a | not stated | not stated | n/a |
| Muse Spark Open (open weights) | not an API id; Baseten slug below | yes (also video, audio) | not stated on Meta's page | not stated | 1,048,576 (Baseten) |
| Muse Glimmer (open weights) | not stated | yes | not stated | not stated | not stated |

Max output tokens: **not stated** for any model. Muse Spark Open and Glimmer are "open-weight and not called over
Model API". Docs say 1.3 is current; examples in the docs mix 1.1/1.2/1.3 as defaults.

## Pricing per 1M tokens (https://dev.meta.ai/docs/pricing-rate-limits)
| Model | Input | Cached input | Output |
|---|---|---|---|
| Muse Spark 1.3 / 1.2 / 1.1 | $1.25 | $0.15 | $4.25 |
| Muse Spark 1.3 / 1.2 Contributor | $0.10 | $0.002 | $0.20 |
| Muse Wonder 1.0 | $4.00 | $0.20 | $20.00 |

No long-context premium. Reasoning tokens are billed as output tokens. Contributor mode is cheaper because prompts and
responses may be used to train future Meta models; our data is synthetic physics scenarios, but this is your call.
Credits and spend limits: **not stated** in the docs I read (one third-party page claims $20 free credit for new accounts).

## Rate limits (per team, per model)
Muse Spark 1.3/1.2/1.1: 3,000 requests/min and 4,000,000 tokens/min. Contributor variants: 150 RPM, 3,000,000 TPM.
Responses carry `x-ratelimit-remaining-*` headers. Baseten (account tiers, not model-specific): 15 RPM / 100k TPM unverified,
120 RPM / 500k TPM verified, 120 RPM / 1M TPM Pro. Baseten recommends about 60 requests/min per session.

## Authentication and endpoints
- Create a key through the "API Login" flow on dev.meta.ai. Docs use env var **`MODEL_API_KEY`** and pass it as `api_key` to an OpenAI SDK client.
- Base URL for all OpenAI-compatible formats: **`https://api.meta.ai/v1`**. Protocols: Responses (recommended), Chat Completions, Messages.
- The raw HTTP header is **not shown** in the docs I read (an OpenAI-compatible bearer token is the likely form). The smoke test will confirm it.
- Baseten: `https://inference.baseten.co/v1`, `Authorization: Bearer <key>`, env var in their example `BASETEN_API_KEY`.

## Tool calling (https://dev.meta.ai/docs/tool-calling)
- Responses API: flat tool defs `{type:"function", name, description, parameters}`; reply items `function_call` with `call_id`; send `function_call_output`; chain with `previous_response_id`.
- Chat Completions: nested `function` key; `choices[0].message.tool_calls`; reply with one `role:"tool"` message per call; full history is stateless.
- **`tool_choice` supports only `"auto"`**; `"required"`, `"none"` and named choices return HTTP 400. Parallel calls default on (`parallel_tool_calls`).
- Function names: `[a-zA-Z0-9_.-]+`, at most one dot. Call ids 1 to 64 chars. Arguments are not guaranteed to match the schema: validate them.

## Structured output (https://dev.meta.ai/docs/structured-output)
Chat Completions `response_format: {type:"json_schema", json_schema:{name, schema}}`; Responses `text.format`. No recursive schemas; depth 10.
**Baseten says structured outputs / JSON mode are supported on all models except Muse Spark Open.**

## Vision (https://dev.meta.ai/docs/image-understanding)
Chat Completions `image_url` parts (URL or base64 `data:` URL), Responses `input_image`. JPEG/PNG/GIF/WebP, 50 MB inline, 50 images per request,
user-role messages only. About 1,300 to 1,500 tokens for a 1280 px image.

## Reasoning
`reasoning_effort` (Chat Completions) or `reasoning.effort` (Responses): minimal, low, medium, high, xhigh, max (`none` returns 400).
**On Chat Completions with external keys, reasoning does not carry across turns** (`reasoning_content` is redacted); the docs recommend Responses for agent loops.

## Open-weight path (Baseten)
- Slug in Baseten docs and library page: **`meta-models/muse-spark-open`**. Your brief says `meta-models/muse-spark-open-1026`. **Resolved by the participant guide:** the docs use `meta-models/muse-spark-open`, but for the hackathon use `meta-models/muse-spark-open-1026`. `configs/open_weight.json` uses the `-1026` slug.
- Baseten serves it through Chat Completions only (no Responses API documented). Image input works through `image_url`. Tool calling: Baseten says all Model APIs support it, but Meta's and Baseten's Muse Spark Open pages do not confirm it for this model. `n>1` returns 400; `seed` / `temperature: 0` do not guarantee reproducibility.
- Price on Baseten's library page: $1.25 input, $0.15 cached, $4.25 output per 1M.
- License: Muse Model License plus the Acceptable Use Policy.

**Can the same code run on both?** Yes, with caveats. Both speak OpenAI Chat Completions, so base URL, model name and API-key env var
can be config. The design therefore uses **Chat Completions with tool calling, replaying the full history each turn** (no `previous_response_id`,
no structured-output dependency), and keeps reasoning effort optional. Not verified until a live call: tool calling on the open model,
whether Baseten accepts `reasoning_effort`, and the exact `-1026` slug.

## Not found
- The hackathon participant guide (submission requirements, bonus-prize eligibility, deadline) was not reachable from public search. I could not
  verify any of it, including the "Best Build on Meta Open Weight Models" criteria. Please paste the relevant section or URL.
- Baseten model-API pricing/limit pages for Muse Spark Open specifically.

## Participant guide (pasted by the user, Oct 27)
- Repo must be public. Max team size 4; solo allowed. 3-minute demo video is a primary judging artifact. Everyone who worked on it is listed on the submission page.
- Deadline: Wed Nov 4, 9:00 PM Eastern (EST after Nov 1), which is Thu Nov 5, 10:00 AM in Singapore. Target: submit by Nov 3 evening ET.
- Demo must show only what was built during the hackathon and judges must be able to identify it (see `WHAT_WE_BUILT.md`).
- Judging: Stage 1 pass/fail (fits theme, reasonably uses Meta models); Stage 2 Impact 30, Meta tooling 25, Demo 25, Technical depth 20.
- Bonuses we could target: Best Build on Meta Open Weight Models ($10K, Muse Spark Open or Glimmer via partner inference; flagged in round one and verified by the partner); Best Agentic Use-Case ($5K); Most Viral (X post link submitted on the platform; engagement counted until Nov 8).
- Credits: Meta $150 (card required; set a Spend Limit notification on Billing). Baseten $100 on request (reviewed, emailed), Modal $100 (first 1,000), Fireworks $25 (shortlist).
- Not building any of the listed anti-projects.
