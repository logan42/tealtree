@echo off

mkdir bin 2>NUL
mkdir obj 2>NUL
erase /Q bin\*

cl.exe /EHsc /MD /Ox ^
 /DNDEBUG ^
 /DGHEAP_CPP11 ^
 /IC:\\work\\boost_1_61_0\\ ^
 /Iinclude ^
 /Fo.\obj\ ^
  src\*.cpp  ^
 /link ^
 /LIBPATH:C:\work\boost_1_61_0\stage\lib ^
 /out:.\bin\tealtree.exe
