# Lab Activity: Stack Operations on PDP-8

## Learning Objectives

By the end of this lab, you will be able to:
1. Implement PUSH and POP operations on the PDP-8
2. Understand stack pointer management and memory layout
3. Detect and handle stack overflow and underflow conditions
4. Use the stack for parameter passing in subroutines
5. Apply stack operations to solve practical programming problems

## Prerequisites

- Familiarity with PDP-8 assembly language
- Understanding of memory-reference instructions (TAD, DCA, ISZ)
- Knowledge of subroutine calls (JMS, JMP I)
- Completion of basic PDP-8 programming exercises

## Stack Fundamentals on the PDP-8

The PDP-8 does not have built-in stack instructions like modern processors. Instead, we implement a software stack using:

- A **stack pointer** (SP) that holds the address of the top of the stack
- A **stack area** in memory reserved for stack storage
- **PUSH operation**: Store a value on the stack and increment SP
- **POP operation**: Decrement SP and retrieve a value from the stack

### Memory Layout Convention

```
Address    Purpose
------     -------
0020       Stack pointer (SP)
0021       Stack base address constant
0022       Stack limit address constant
0300-03FF  Stack storage area (256 words)
```

The stack grows upward in memory. SP points to the next available location.

---

## Experiment 1: Implementing Basic PUSH Operation

### Objective
Implement a PUSH subroutine that stores the accumulator value onto the stack.

### Theory
The PUSH operation must:
1. Store the current AC value at the location pointed to by SP
2. Increment SP to point to the next available location
3. Return to the caller

### Sample Code

```assembly
/ stack-push.asm
/ Demonstrates basic PUSH operation

        *0200

START,  CLA CLL
        TAD STACK_BASE          / Initialize stack pointer
        DCA SP
        
        / Push three values onto the stack
        CLA
        TAD VALUE1              / Load first value
        JMS PUSH                / Push it
        
        TAD VALUE2              / Load second value  
        JMS PUSH                / Push it
        
        TAD VALUE3              / Load third value
        JMS PUSH                / Push it
        
        HLT                     / Done

/------------------------------------------------------------
/ PUSH - Push AC onto stack
/ Entry: AC contains value to push
/ Exit: AC cleared, SP incremented
/------------------------------------------------------------
PUSH,   0
        DCA I SP                / Store AC at address in SP
        ISZ SP                  / Increment stack pointer
        7000                    / NOP (no operation)
        JMP I PUSH              / Return

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,         0                   / Stack pointer
STACK_BASE, 0300                / Base of stack area

VALUE1,     0001                / Test value 1
VALUE2,     0002                / Test value 2
VALUE3,     0003                / Test value 3

        *0300                   / Stack storage area
STACK,  0                       / Stack starts here
```

### Tasks

1. Assemble and run the program:
   ```bash
   python3 tools/pdp8_asm.py docs/examples/stack-push.asm docs/examples/stack-push.srec
   ./monitor docs/examples/stack-push.srec
   ```

2. In the monitor, examine the stack area after execution:
   ```
   pdp8> switch load 0200
   pdp8> c 100
   pdp8> mem 0300 8
   ```

3. Verify that locations 0300, 0301, and 0302 contain 0001, 0002, and 0003.

### Questions

1. What is the final value of the stack pointer (location 0020)?
2. What would happen if you continued pushing values beyond address 0377?
3. Why do we use `DCA I SP` instead of `DCA SP`?
4. Modify the code to push five values instead of three. What changes are needed?

---

## Experiment 2: Implementing Basic POP Operation

### Objective
Implement a POP subroutine that retrieves a value from the stack into the accumulator.

### Theory
The POP operation must:
1. Decrement SP to point to the last stored value
2. Load that value into AC
3. Return to the caller

Since the PDP-8 doesn't have a "decrement" instruction, we use TAD with a negative constant.

### Sample Code

```assembly
/ stack-pop.asm
/ Demonstrates PUSH and POP operations

        *0200

START,  CLA CLL
        TAD STACK_BASE          / Initialize stack pointer
        DCA SP
        
        / Push three values
        TAD VALUE1
        JMS PUSH
        TAD VALUE2
        JMS PUSH
        TAD VALUE3
        JMS PUSH
        
        / Pop three values and store them
        JMS POP
        DCA RESULT3
        JMS POP
        DCA RESULT2
        JMS POP
        DCA RESULT1
        
        HLT

/------------------------------------------------------------
/ PUSH - Push AC onto stack
/------------------------------------------------------------
PUSH,   0
        DCA I SP
        ISZ SP
        7000                    / NOP
        JMP I PUSH

/------------------------------------------------------------
/ POP - Pop value from stack into AC
/ Entry: Stack must not be empty
/ Exit: AC contains popped value, SP decremented
/------------------------------------------------------------
POP,    0
        CLA                     / Clear AC
        TAD SP                  / Load current SP
        TAD NEG1                / Subtract 1
        DCA SP                  / Store decremented SP
        TAD I SP                / Load value from stack
        JMP I POP               / Return with value in AC

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,         0
STACK_BASE, 0300
NEG1,       7777                / -1 in two's complement

VALUE1,     0111                / 'I' in octal ASCII
VALUE2,     0110                / 'H' in octal ASCII  
VALUE3,     0041                / '!' in octal ASCII

RESULT1,    0
RESULT2,    0
RESULT3,    0

        *0300
STACK,  0
```

### Tasks

1. Assemble and run the program.

2. Examine the results after execution:
   ```
   pdp8> mem 0220 8
   ```

3. Verify that RESULT1, RESULT2, and RESULT3 contain the values in reverse order.

### Questions

1. Why do the results appear in reverse order (RESULT3 has VALUE1)?
2. What is the LIFO (Last In, First Out) principle, and how does this code demonstrate it?
3. What is the value of SP after all pops? Does it match the initial value?
4. What happens if you try to POP from an empty stack?

---

## Experiment 3: Stack Overflow and Underflow Detection

### Objective
Add boundary checking to prevent stack overflow (pushing beyond stack limit) and underflow (popping from empty stack).

### Theory
Safe stack operations require:
- **Overflow check**: Before PUSH, verify that SP < STACK_LIMIT
- **Underflow check**: Before POP, verify that SP > STACK_BASE

If a boundary is violated, the operation should halt or handle the error gracefully.

### Sample Code

```assembly
/ stack-safe.asm
/ Stack with overflow and underflow detection

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Try to push values
        TAD VAL1
        JMS PUSH_SAFE
        TAD VAL2
        JMS PUSH_SAFE
        
        / Try to pop values (should succeed)
        JMS POP_SAFE
        DCA TEMP
        JMS POP_SAFE
        DCA TEMP
        
        / Try to pop again (should fail - underflow)
        JMS POP_SAFE
        DCA TEMP
        
        HLT                     / Should not reach here if underflow handled

/------------------------------------------------------------
/ PUSH_SAFE - Push with overflow checking
/ Entry: AC contains value to push
/ Exit: AC cleared, or halts on overflow
/------------------------------------------------------------
PUSH_SAFE, 0
        DCA TEMP                / Save value temporarily
        
        / Check if SP < STACK_LIMIT
        CLA
        TAD STACK_LIMIT
        CMA                     / Complement
        IAC                     / Increment (two's complement negate)
        TAD SP                  / SP - STACK_LIMIT
        SMA SZA                 / Skip if negative or zero
        JMP OVERFLOW            / Stack full!
        
        TAD TEMP                / Restore value
        DCA I SP                / Store on stack
        ISZ SP
        7000                    / NOP
        JMP I PUSH_SAFE

OVERFLOW,
        HLT                     / Halt on overflow
        JMP OVERFLOW            / Loop if continued

/------------------------------------------------------------
/ POP_SAFE - Pop with underflow checking
/ Entry: Stack should not be empty
/ Exit: AC contains value, or halts on underflow  
/------------------------------------------------------------
POP_SAFE, 0
        / Check if SP > STACK_BASE
        CLA
        TAD SP
        TAD NEG_STACK_BASE      / SP - STACK_BASE
        SZA                     / Skip if zero (empty)
        JMP POP_OK
        JMP UNDERFLOW           / Stack empty!
        
POP_OK, CLA
        TAD SP
        TAD NEG1
        DCA SP
        TAD I SP
        JMP I POP_SAFE

UNDERFLOW,
        HLT                     / Halt on underflow
        JMP UNDERFLOW

/------------------------------------------------------------
/ Data  
/------------------------------------------------------------
SP,              0
STACK_BASE,      0300
STACK_LIMIT,     0400          / 256 words available
NEG_STACK_BASE,  7500          / -0300 in octal
NEG1,            7777          / -1

VAL1,            0123
VAL2,            0456
TEMP,            0

        *0300
STACK,  0
```

### Tasks

1. Run the program and observe where it halts (on the underflow).

2. Modify the code to attempt overflow:
   - Remove the two POP operations
   - Add a loop that pushes 257 values onto the stack
   - Observe the overflow halt

3. Implement error codes:
   - Instead of halting, store an error code (e.g., 7777 for underflow, 7776 for overflow)
   - Return the error code in AC
   - Let the caller check AC and handle the error

### Questions

1. Why do we need both overflow and underflow checks?
2. What are the trade-offs between halting on error versus returning an error code?
3. How much memory overhead does boundary checking add?
4. Could you use the Link bit as an error flag instead of AC?

---

## Experiment 4: Using Stack for Subroutine Parameters

### Objective
Use the stack to pass parameters to a subroutine and return results, demonstrating a calling convention.

### Theory
Modern calling conventions use the stack for:
- Passing parameters to functions
- Storing return values
- Preserving local variables

On the PDP-8, we can implement a simple convention:
1. Caller pushes parameters onto the stack (right to left)
2. Callee retrieves parameters from the stack
3. Callee pushes result onto the stack
4. Caller pops the result

### Sample Code

```assembly
/ stack-params.asm
/ Passing parameters via stack

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Calculate (5 + 3) using ADD_NUMS subroutine
        TAD NUM1                / First parameter
        JMS PUSH
        TAD NUM2                / Second parameter
        JMS PUSH
        
        JMS ADD_NUMS            / Call subroutine
        
        JMS POP                 / Get result
        DCA RESULT
        
        HLT

/------------------------------------------------------------
/ ADD_NUMS - Add two numbers from stack, push result
/ Stack on entry: [bottom] ... param1 param2 [top]
/ Stack on exit:  [bottom] ... result [top]
/------------------------------------------------------------
ADD_NUMS, 0
        / Pop second parameter
        JMS POP
        DCA PARAM2
        
        / Pop first parameter  
        JMS POP
        DCA PARAM1
        
        / Add them
        CLA
        TAD PARAM1
        TAD PARAM2
        
        / Push result
        JMS PUSH
        
        JMP I ADD_NUMS

/------------------------------------------------------------
/ PUSH and POP subroutines (same as Experiment 2)
/------------------------------------------------------------
PUSH,   0
        DCA I SP
        ISZ SP
        7000                    / NOP
        JMP I PUSH

POP,    0
        CLA
        TAD SP
        TAD NEG1
        DCA SP
        TAD I SP
        JMP I POP

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,         0
STACK_BASE, 0300
NEG1,       7777

NUM1,       0005
NUM2,       0003
RESULT,     0

PARAM1,     0
PARAM2,     0

        *0300
STACK,  0
```

### Tasks

1. Run the program and verify RESULT contains 0010 (octal) = 8 (decimal).

2. Extend the ADD_NUMS subroutine:
   - Create MULTIPLY_NUMS that multiplies two parameters
   - Create SUBTRACT_NUMS that subtracts param2 from param1

3. Create a compound expression:
   - Calculate (A + B) * C using stack parameters
   - Push A, B; call ADD_NUMS; push C; call MULTIPLY_NUMS

### Questions

1. What happens if parameters are pushed in the wrong order?
2. How does this calling convention compare to using dedicated parameter locations?
3. What are the advantages of stack-based parameter passing?
4. How would you pass more than two parameters?

---

## Experiment 5: Reverse String Using Stack

### Objective
Implement a practical application that uses the stack to reverse a string.

### Theory
A stack's LIFO property makes it perfect for reversing sequences:
1. Push each character onto the stack
2. Pop each character back (they come out in reverse order)
3. Store or print the reversed characters

This demonstrates a real-world use of stack operations.

### Sample Code

```assembly
/ stack-reverse.asm
/ Reverse a string using stack operations

        *0200

START,  CLA CLL
        TAD STACK_BASE
        DCA SP
        
        / Push each character of the string
        CLA
        TAD STRING_PTR
        TAD NEG1                / Adjust for auto-increment
        DCA PTR
        
PUSH_LOOP,
        TAD I PTR               / Get character (auto-increment)
        SZA                     / Check for null terminator
        JMP DO_PUSH
        JMP POP_START           / Done pushing, start popping
        
DO_PUSH,
        JMS PUSH                / Push character
        ISZ PTR                 / Already incremented by I PTR
        7000                    / NOP
        JMP PUSH_LOOP

POP_START,
        / Pop characters and store in output buffer
        CLA
        TAD OUTPUT_PTR
        TAD NEG1
        DCA PTR
        
POP_LOOP,
        / Check if stack is empty
        CLA
        TAD SP
        TAD NEG_STACK_BASE
        SZA
        JMP DO_POP
        JMP DONE                / Stack empty, we're done
        
DO_POP,
        JMS POP                 / Pop character
        DCA I PTR               / Store in output (auto-increment)
        ISZ PTR
        7000                    / NOP
        JMP POP_LOOP

DONE,   / Add null terminator to output
        CLA
        DCA I PTR
        
        HLT

/------------------------------------------------------------
/ PUSH and POP (same as before)
/------------------------------------------------------------
PUSH,   0
        DCA I SP
        ISZ SP
        7000                    / NOP
        JMP I PUSH

POP,    0
        CLA
        TAD SP
        TAD NEG1
        DCA SP
        TAD I SP
        JMP I POP

/------------------------------------------------------------
/ Data
/------------------------------------------------------------
SP,              0
STACK_BASE,      0300
NEG_STACK_BASE,  7500
NEG1,            7777
PTR,             0

STRING_PTR,      STRING
OUTPUT_PTR,      OUTPUT

        *0240
STRING,
        0110                    / H
        0145                    / e
        0154                    / l
        0154                    / l
        0157                    / o
        0000                    / null terminator

        *0250
OUTPUT, 0                       / Output buffer (6 words + null)
        0
        0
        0
        0
        0
        0

        *0300
STACK,  0
```

### Tasks

1. Run the program and examine the OUTPUT buffer:
   ```
   pdp8> mem 0250 8
   ```

2. Verify it contains "olleH" (0157 0154 0154 0145 0110 0000).

3. Modify the program to print the reversed string to the console:
   - Add a PUTCH routine (see `demo/dull-boy.asm` for reference)
   - Add a loop that outputs each character from OUTPUT

4. Handle longer strings:
   - Increase the stack size
   - Test with the string "PDP-8 Stack Operations"

### Questions

1. Why is this more efficient than swapping characters in place?
2. What is the space complexity of this algorithm?
3. How would you modify this to reverse words within a sentence (not individual characters)?
4. What would happen if the string is longer than the stack capacity?

---

## Advanced Exercises

### Exercise A: Recursive Fibonacci
Implement a recursive Fibonacci function using the stack to save state:
- Push the parameter N onto the stack
- If N ≤ 1, return N
- Otherwise, recursively calculate fib(N-1) + fib(N-2)
- Use the stack to preserve N across recursive calls

### Exercise B: Expression Evaluation
Implement a postfix (RPN) calculator:
- Input: "3 4 + 2 *" (means (3+4)*2 = 14)
- Push operands onto the stack
- When an operator is encountered, pop operands, compute, and push result

### Exercise C: Nested Subroutine Calls
Test stack reliability with deeply nested calls:
- Create subroutines A, B, C that call each other
- Each subroutine pushes a marker value
- Verify all markers are preserved correctly

### Exercise D: Balanced Parentheses Checker
Use the stack to verify balanced parentheses in a string:
- Push '(' when encountered
- Pop when ')' is encountered
- String is balanced if stack is empty at the end

---

## Lab Report Guidelines

Your lab report should include:

1. **Introduction**: Brief overview of stack operations and their importance

2. **Methods**: For each experiment:
   - Your implementation (assembly code)
   - Any modifications or enhancements you made
   - Test cases you ran

3. **Results**: For each experiment:
   - Monitor output or memory dumps
   - Answer to each question
   - Any unexpected behavior observed

4. **Discussion**:
   - Challenges you encountered
   - Comparison of software stack vs. hardware stack instructions
   - Ideas for optimizing the stack operations

5. **Conclusion**:
   - Summary of what you learned
   - Real-world applications of stack operations

## Additional Resources

- `docs/pdp8-programmer-guide.md` - PDP-8 programming reference
- `docs/pdp8-opcodes-cheat-sheet.md` - Complete instruction set
- `demo/dull-boy.asm` - Example of subroutine usage
- `demo/core.asm` - BIOS call conventions

## Grading Rubric

- Experiment 1 (PUSH): 15 points
- Experiment 2 (POP): 15 points  
- Experiment 3 (Safety): 20 points
- Experiment 4 (Parameters): 20 points
- Experiment 5 (Reverse String): 20 points
- Lab Report: 10 points

**Total: 100 points**

---

*This lab activity is designed for the Waffle8 PDP-8 emulator. All code examples follow the conventions documented in the repository's programmer guide.*
