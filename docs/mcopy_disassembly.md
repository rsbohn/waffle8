# Disassembly of mcopy.srec

## Address 0300-0307: MCOPY routine
```
0300: 0000    (data)      / Return address storage
0301: 7300    CLA CLL     / Clear AC and Link
0302: 2307    ISZ 0307    / Increment COUNT, skip if zero
0303: 5700    JMP I 0300  / Jump indirect through return address (exit)
0304: 1410    TAD I 0010  / Load from address in location 0010 (auto-increment)
0305: 3411    DCA I 0011  / Store to address in location 0011 (auto-increment)  
0306: 5302    JMP 0302    / Jump back to loop start
0307: 7744    (data)      / COUNT constant (-0344 in two's complement)
```

## Instruction Breakdown:
- **0300**: Return address slot (filled by JMS caller)
- **0301**: `CLA CLL` - Clear accumulator and link
- **0302**: `ISZ 0307` - Increment COUNT (stored at 0307), skip when zero
- **0303**: `JMP I 0300` - Return via stored address
- **0304**: `TAD I 0010` - Load word from address stored in location 0010 (auto-increments location 0010)
- **0305**: `DCA I 0011` - Store word to address stored in location 0011 (auto-increments location 0011)
- **0306**: `JMP 0302` - Loop back to count check

## Correct Usage:
The routine expects:
- **Location 0010**: Contains source address (auto-incremented by TAD I 0010)
- **Location 0011**: Contains destination address (auto-incremented by DCA I 0011)
- **Location 0307**: Contains negative count (incremented by ISZ until zero)

This is a self-contained routine where COUNT is embedded at address 0307, and the caller sets up locations 0010 and 0011 with the source and destination addresses.
```
0300: 0000    (data)      / Return address storage
0301: 7300    CLA CLL     / Clear AC and Link
0302: 2307    ISZ 0307    / Increment COUNT, skip if zero
0303: 5700    JMP I 0300  / Jump indirect through return address (exit)
0304: 1410    TAD I 0010  / Load from address in SRC (auto-increment)
0305: 3411    DCA I 0011  / Store to address in DEST (auto-increment)  
0306: 5302    JMP 0302    / Jump back to loop start
0307: 7744    (data)      / Data word (part of constants)
```

## Instruction Breakdown:
- **0300**: Return address slot (filled by JMS caller)
- **0301**: `CLA CLL` - Clear accumulator and link
- **0302**: `ISZ 0307` - This should be `ISZ COUNT` but shows as 0307
- **0303**: `JMP I 0300` - Return via stored address
- **0304**: `TAD I 0010` - Load word from SRC location (with auto-increment)
- **0305**: `DCA I 0011` - Store word to DEST location (with auto-increment)
- **0306**: `JMP 0302` - Loop back to count check

## Note:
The ISZ instruction at 0302 shows `ISZ 0307` instead of `ISZ COUNT`. This suggests the assembler resolved the COUNT symbol to address 0307, but based on our source code, COUNT should be at address 0012 (octal).