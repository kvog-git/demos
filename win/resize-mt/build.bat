@echo off
if not exist bin mkdir bin
cl /W4 /Od /Zi /Fe:bin\win-resize-mt.exe win-resize-mt.c /Fd:bin\ /Fo:bin\
