# Inventory Encoder State Machine

This document captures a compact state machine for the "inventory" encoder: a
streaming tokenizer that reads characters from stdin, emits whitespace-delimited
tokens, and converts them into SIXBIT words augmented with marker records for
dates and numeric quantities. The example trace encodes the phrase `inventory
eggs milk mix` with a single date and fixed per-item counts.

## Encoding Primitives

- **Word token** – Extract the first four ASCII letters from the word, uppercase
  them, and encode as two SIXBIT characters per 12-bit word (padding with space
  when the letter count is odd). Example: `EGGS` → `0607 0723`.
- **Date marker** – Emit `7070` followed by two parameter words:
  - Year as plain octal (e.g., `3751` for 2025).
  - Month/day packed as `month * 32 + day` (e.g., `0537` for October 31).
- **Number marker** – Emit `7071` followed by a single octal quantity (e.g.,
  `0220` for 144 decimal).
- **Categories** – Valid category keywords are `inventory`, `receiving`,
  `warehouse`, and `kitchen`. Each is encoded using the same word-token rule;
  they occupy the leading position in the stream.

## States and Transitions

| State            | On Input                                     | Action                                                                   | Next State       |
|------------------|----------------------------------------------|--------------------------------------------------------------------------|------------------|
| `EXPECT_CATEGORY`| Category token (`inventory`, `receiving`, `warehouse`, `kitchen`) | Encode word as SIXBIT pair (e.g., `INVE` → `1156 2606`).                 | `EXPECT_DATE`    |
| `EXPECT_DATE`    | Date payload                                 | Emit `7070`, then year and month/day words.                              | `EXPECT_ITEM`    |
| `EXPECT_ITEM`    | Word token                                   | Encode word as SIXBIT pair.                                              | `EXPECT_NUMBER`  |
| `EXPECT_NUMBER`  | Quantity payload                             | Emit `7071`, then the octal quantity.                                    | `EXPECT_ITEM`    |
| `EXPECT_ITEM`    | End-of-input                                 | Halt (stream complete).                                                  | —                |

Notes:

- Date payload can be provided as `(year, month, day)` integers or precomputed
  words; the encoder is responsible for packing them before emission.
- Quantity payload repeats for every item; the machine continues alternating
  between `EXPECT_ITEM` and `EXPECT_NUMBER` until the input is exhausted.
- The encoder must reject any category token outside the defined list.
- Token boundaries are determined by whitespace; the character stream can be
  arbitrarily long and the state machine processes characters incrementally.

## Example Trace

Encoding the sample sequence yields the following words (comments added for
clarity):

```
1156 2606    # EXPECT_CATEGORY – "INVE" category
7070         # EXPECT_DATE – date marker
3751 0537    # EXPECT_DATE – 2025, (10 * 32 + 31)
0607 0723    # EXPECT_ITEM – "EGGS"
7071         # EXPECT_NUMBER – quantity marker
0220         # EXPECT_NUMBER – 144 units
1511 3000    # EXPECT_ITEM – "MIX" (padded with space)
7071         # EXPECT_NUMBER – quantity marker
0220         # EXPECT_NUMBER – 144 units
1511 1413    # EXPECT_ITEM – "MILK"
7071         # EXPECT_NUMBER – quantity marker
0220         # EXPECT_NUMBER – 144 units
```

The encoder stops in `EXPECT_ITEM` once it runs out of words. Additional
inventory entries would continue alternating between `EXPECT_ITEM` and
`EXPECT_NUMBER`, reusing the same marker conventions.
