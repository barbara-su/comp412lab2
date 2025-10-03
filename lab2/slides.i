// Example from Slides for the allocation video 
//SIM INPUT:  -i 128 1 2 3
//OUTPUT:  7

loadI  128   => r1
load   r1    => r2
loadI  132   => r7
load   r7    => r4
loadI  136   => r6
load   r6    => r5
mult   r4, r5 => r3
add    r2, r3 => r0
store  r0     => r1
