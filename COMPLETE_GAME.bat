@echo off

TITLE PSPQuakeII Builder

mkdir PSPQuakeII
mkdir PSPQuakeII\baseq2
mkdir PSPQuakeII\rogue
mkdir PSPQuakeII\xatrix
mkdir PSPQuakeII\crbot
mkdir PSPQuakeII\action

echo Dirs - OK

cd game
make -f Makefile
cd ..\
copy game\gamepsp.prx PSPQuakeII\baseq2\gamepsp.prx

echo BASE GAME - OK

cd rogue
make -f Makefile
cd ..\
copy rogue\gamepsp.prx PSPQuakeII\rogue\gamepsp.prx

echo ROGUE - OK

cd xatrix
make -f Makefile
cd ..\
copy xatrix\gamepsp.prx PSPQuakeII\xatrix\gamepsp.prx

echo XATRIX - OK

cd crbotsource
make -f Makefile
cd ..\
copy crbotsource\gamepsp.prx PSPQuakeII\crbot\gamepsp.prx

echo CRBOT - OK

cd action
make -f Makefile
cd ..\
copy action\gamepsp.prx PSPQuakeII\action\gamepsp.prx

echo ACTION - OK

echo done...

pause