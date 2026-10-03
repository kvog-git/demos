@echo off
if not exist bin mkdir bin
cl /W4 /Od /Zi /Fe:bin\win-resize-st.exe win-resize-st.c /Fd:bin\ /Fo:bin\
