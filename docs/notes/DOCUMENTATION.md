# Documentation organization and export checks

The explanatory entry points are `docs/README.md`, `HOW_IT_WORKS.md`,
`ARCHITECTURE.md` and `BUILDING_AND_TESTING.md`. Edition and mod documents
describe plans explicitly. Historical implementation findings live under
`docs/notes/`, including reference-tool records under `notes/analysis/`.

Stable explanatory pages remain at paths referenced by source comments.
This permits documentation relocation without editing application or test
code. Markdown links in moved records are resolved relative to their new
location. The local event-inventory writer uses the notes directory too.

## Source preservation

The organization check compared file hashes for 583 files in `src/`,
`include/` and `tests/` and found no changes. All 48 literal documentation
references in those directories resolved. The root README and private
handoffs were preserved byte for byte. All 535 grade-table lines in the
private fidelity record were preserved exactly, ignoring line terminators.
No grades or game behavior were changed by this work.

## Export contract

`tools/export_public.py` exports committed HEAD. Public documentation and
the helper must be committed; unrelated local source changes do not enter
the exported tree. The helper never commits or pushes the destination.

Private instructions, handoffs and the disc-derived reward inventory are
excluded. Public guides and development notes are included. The only text
transformation redacts private wording in exported fidelity tables, because
the source tables must remain intact. It preserves grades, numeric evidence,
test counts, function addresses and tolerances.

Before replacing destination files, the helper checks forbidden traces,
UTF-8/LF documentation and relative Markdown links against the actual public
file set. Failed validation leaves the destination unchanged.

## Comparator checks

The positive content/link check passed. Four intentional fixtures were
rejected: private wording, a nonexistent Markdown destination, a link to an
excluded private document, and CRLF documentation. These checks concern
documentation packaging, not game fidelity or rendering equivalence.
