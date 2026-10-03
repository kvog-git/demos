@echo off
if not exist bin mkdir bin
cl /W4 /Od /Zi /Fe:bin\flacdec.exe flacdec.c /Fd:bin\ /Fo:bin\
