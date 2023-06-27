
CC=cl
ODIR=obj
IDIR=include W:/jadel2/export/include
_DEPS=include
DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))
CFLAGS=/I$(IDIR)
_OBJ=*.obj
OBJ=$($(wildcard patsubst %,$(ODIR)/%,$(_OBJ)))


source:=$(wildcard src/*.cpp)
objects := $(patsubst src/%.cpp,obj/%.obj,$(wildcard src/*.cpp))

SCRDIR=src
.PHONY:
all: stars

withjadel:
	(cd W:/jadel2 && make clean && make)
	copy W:\jadel2\export\lib\jadel.dll bin\jadel.dll
	$(CC) /c /Zi /EHsc /Iinclude /IW:/jadel2/export/include /std:c++latest ./src/*.cpp /Foobj/ 
	$(CC) /Zi obj/*.obj W:/jadel2/export/lib/jadelmain.lib W:/jadel2/export/lib/jadel.lib /Febin/stars.exe /link /SUBSYSTEM:WINDOWS

stars: $(objects)
	$(CC) /Zi $^ W:/jadel2/export/lib/jadelmain.lib W:/jadel2/export/lib/jadel.lib /Febin/$@ /link /SUBSYSTEM:WINDOWS

#$(objects): $(source)
#	$(CC) /c /Zi /EHsc /Iinclude /IW:/jadel2/export/include /std:c++latest $^ /Foobj/

obj/%.obj: src/%.cpp
	$(CC) /c /Zi /EHsc /Iinclude /IW:/jadel2/export/include /std:c++latest $^ /Foobj/

copyjadel:
	$(CC)  /c /Zi /EHsc /Iinclude /IW:/jadel2/export/include /std:c++latest ./src/*.cpp /Foobj/ 
	$(CC)  /Zi obj/*.obj W:/jadel2/export/lib/jadelmain.lib W:/jadel2/export/lib/jadel.lib /Febin/stars.exe /link /SUBSYSTEM:WINDOWS
	copy W:\jadel2\export\lib\jadel.dll bin\jadel.dll

.PHONY:
clean:
	del obj\\*.obj bin\\*.exe