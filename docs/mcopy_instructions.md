# Manual Setup to Copy 0344 words from 0200 to 4000

To use the mcopy routine to copy 0344 (octal) words from address 0200 to address 4000:

## Step 1: Load the mcopy routine
```
read demo/mcopy.srec
```

## Step 2: Set up the parameters in zero page
```
# Set COUNT = -0344 (negative of number of words to copy)
write 0012 7434        # -0344 in two's complement

# Set SRC = 0200 (source address)  
write 0010 0200        # Source address

# Set DEST = 4000 (destination address)
write 0011 4000        # Destination address
```

## Step 3: Call the routine
```
# Set PC to mcopy entry point and run
pc 0300
run
```

## What happens:
- mcopy reads from address in SRC (0010), auto-increments SRC
- mcopy writes to address in DEST (0011), auto-increments DEST  
- COUNT (0012) increments from -0344 toward 0
- When COUNT reaches 0, ISZ skips and routine returns
- Total: 0344 (228 decimal) words copied from 0200-0543 to 4000-4343

## Monitor Commands Summary:
```
read demo/mcopy.srec
write 0012 7434
write 0010 0200  
write 0011 4000
pc 0300
run
```

## Notes:
- 0344 octal = 228 decimal words
- Source range: 0200 to 0543 (octal)
- Destination range: 4000 to 4343 (octal)
- Uses auto-increment addressing for efficient copying