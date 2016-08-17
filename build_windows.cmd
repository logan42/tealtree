@echo off

mkdir bin 2>NUL
mkdir obj 2>NUL
erase /Q bin\*

cl.exe /EHsc /MD /Ox ^
 /DNDEBUG ^
 /DGHEAP_CPP11 ^
 /Iinclude ^
 /Fo.\obj\ ^
  src\*.cpp  ^
 /link ^
 /out:.\bin\tealtree.exe
