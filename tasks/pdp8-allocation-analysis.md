# Task: PDP-8 Core Memory Allocation Analysis

## Purpose
Examine PDP-8 memory usage by page and describe where we still have room.
We treat each page as 0o200 (128 words) and render two 64-word rows:
- First row: base .. base+0o077
- Second row: base+0o100 .. base+0o177
`#` = an assembled word is present at that address
`.` = unused space
(See tools/allocated.py for implementation details.)

## Command to run
```bash
python tools/allocated.py demo/cal3.asm --all-pages
```

## What to extract from the output
For each page:
1. Percentage used in the first half (base .. base+0o077).
2. Percentage used in the second half (base+0o100 .. base+0o177).
3. Whether the page is:
   - "packed" ( >90% full both halves ),
   - "active with tail" (front is dense but trailing region is mostly '.'), or
   - "sparse / available".

## What to report
1. A short narrative per page, like:
   - `Page 03 (0600-0777): completely saturated. No free space.`
   - `Page 04 (1000-1177): first half full, second half mostly free. Candidate for buffers.`

2. A consolidation section:
   - Pages under memory pressure (>=90% full overall).
   - Pages that offer safe free regions for new code or runtime data.
   - Approximate "best place to stash scratch/buffers now".

## Output format
Return Markdown with:
- "Per-Page Analysis" section
- "Recommendations" section
- Then include the raw page map in a fenced code block.

## Notes / Conventions
- Use octal addresses in the summary when referring to ranges.
- When suggesting locations for scratch/buffers, prefer the free tail of higher pages (e.g. 1100+, 1300+, 1500+) so we don't fragment low core.
- Page 00 is special (zero page, autoindex locations); call out any remaining slack at the very low addresses if applicable.
