// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/04/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel;
// the screen should remain fully black as long as the key is pressed. 
// When no key is pressed, the program clears the screen, i.e. writes
// "white" in every pixel;
// the screen should remain fully clear as long as no key is pressed.

// Put your code here.

@8192
D = A
@SCREEN
D = D + A

@EOF
M = D

(LOOP)
@KBD
D = M
@UP
D; JEQ
@DOWN
0; JMP

(UP)
@SCREEN
D = A

@i
M = D

(WHITEN)
@i
D = M
A = D
M = 0

@i
MD = M + 1

@EOF
D = M - D

@WHITEN
D; JNE

@LOOP
0; JMP

(DOWN)
@SCREEN
D = A

@i
M = D

(DARKEN)
@i
D = M
A = D
D = 0
M = !D

@i
MD = M + 1

@EOF
D = M - D

@DARKEN
D; JNE

@LOOP
0; JMP