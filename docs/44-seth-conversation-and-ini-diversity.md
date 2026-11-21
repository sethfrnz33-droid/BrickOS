# Seth natural conversation and TunerStudio INI diversity — implementation notes

**Branch:** `feature/seth-natural-guide-ini-diversity`.
**Scope:** read-only conversation, education, character animations, and capability visibility.
**NOT:** a general-purpose ECU transport implementation, complete expert-system curriculum,
verified diagnostics, or permission to edit calibration automatically.

## Seth is Seth

- No clone, imprisonment or ECU-resident character mythology in the displayed guide.
- The default phone guide uses plain language first and optional technical details.
- The original head/jaw/mouth assets now have seven CSS-based ambient expressions:
  blink, curious, bored, skeptical, chuckle, surprised and look-away.
  The expressions vary during dialogue and respect reduced-motion preference.
- Thirty chapter questions have been rewritten to ask concrete, approachable questions.
  A response of `Not sure yet` is valid. A user-reported check never certifies an engine.
- The Guide's question box accepts text up to 240 characters. Existing question
  routes still work, and named settings are looked up from the loaded definition
  rather than a fixed ECU feature list.

## Setting explanations

`web/guide-setting-help.mjs` implements a deterministic, bounded explanation
library for common setting families: injector size, dead time, base fuel/VE,
mixture, fuel pressure, spark/dwell, trigger, idle, warmup, throttle, load
pressure, knock, corrections, and sensor inputs.

It explains what a setting name **might** refer to and what context is still
needed. A name alone does not establish that the function exists, is enabled,
uses a particular scaling formula, or has a safe numeric value. Unknown labels
are explicitly unresolved. Text questions do **not** generate ECU read/write
commands or form the basis for a fuzzy automatic editor jump.

The guide's title, badges, chapter language and status favor concepts people
recognize; the **technical ECU definition ID is in an expandable details
section**, not the conversation itself.

## Source and protocol compatibility

The TunerStudio INI is an **ECU definition**, not the ECU or a calibration
proof. `web/ini-compat.mjs` inspects source sections, recognizes family hints
when present and reports declared command-template categories without sending
any commands. It can inventory a definition and explicitly says when read/
write protocol integration is unavailable.

| ECU family | Definition text / name inspection | ESP live read/write |
| --- | --- | --- |
| rusEFI | Existing parser plus source inventory | Existing rusEFI transport, only on confirmed matching identity and supported fields |
| Speeduino | Read-only definitions, subject to parser limitations | **Not yet implemented**; requires a separately verified protocol adapter |
| MegaSquirt MS1/MS2/MS3 | Read-only definitions, subject to parser limitations | **Not yet implemented**; requires platform/version-specific protocol adapters |
| Other TunerStudio ECU | Best-effort read-only definition inspection | **Unsupported until tested** |

The current parser handles a subset of common generated definitions and is
not a complete TunerStudio evaluator. Macro expressions, conditional pages,
multiple command protocols, page identifiers, live channel envelopes,
endianness, field aliases, output lookup formulas and controller-specific
write acknowledgements must be verified for every family before offering
ECU communication. Existing BrickOS USB uses a rusEFI-specific request
envelope; **using another INI does not make the protocol compatible**.

The existing 3DS app on this GitHub branch is older than the user's separate
v104 package. This change does not falsely overwrite that project or claim
3DS-side integration. Future consoles should receive verified, resolved,
portable packet content from the ESP rather than interpreting foreign
command templates locally.

## Tests & deployment

Run from repo root:

```sh
node --test firmware/BrickOS/tests/seth-guide.test.mjs firmware/BrickOS/tests/seth-natural-language.test.mjs
node --test firmware/BrickOS/tests/seth-esp-http-contract.test.mjs
make -C prototype/seth-agent clean test
python3 firmware/BrickOS/tools/generate_web_assets.py
cd firmware/BrickOS && idf.py build
```

Browser on phone, ESP-IDF binary size, actual 3DS client, real ECU access,
and all foreign ECU protocols remain hardware/release checks. Do not call
any untested ECU family supported solely because its INI could be opened.
