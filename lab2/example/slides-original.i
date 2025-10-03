// Example from Slides for the allocation video 
//SIM INPUT:  -i 128 1 2 3
//OUTPUT:  7

loadI  128    => r0
load   r0     => r1
loadI  132    => r2
load   r2     => r3
loadI  136    => r4
load   r4     => r5
mult   r3, r5 => r3
add    r1, r3 => r1
store  r1     => r0
output 128            // should write a seven 
