CC = gcc
LIBS = -lm


default:
	make imageBuilder
	make image

imageBuilder:
	@cp ../src/*.c source
	@cp ../include/*.h source
	@cp source/build_image.h source/build.h
	@rm source/main.c
	@rm source/wifi_connect.*
	@cd source && $(CC) -c memory.c names.c news.c interp.c process.c primitive.c filein.c lex.c parser.c unixio.c st.c initial.c uart.c tty.c
	@cd source && $(CC) memory.o names.o news.o interp.o process.o primitive.o filein.o lex.o parser.o unixio.o st.o uart.o tty.o -o ../stje $(LIBS)
	@cd source && $(CC) memory.o names.o news.o interp.o process.o primitive.o filein.o lex.o parser.o unixio.o initial.o uart.o tty.o -o ../imageBuilder $(LIBS)

image:
	./buildImage basic.st mag.st collect.st file.st mult.st oled.st m5StickC.st tty.st
	@cp ./systemImage ../data/systemImage
	@cp ./objectTable ../data/objectTable
	@cp ./objectData ../data/objectData
	@cp ./objectData ../objects/objectData
	@echo Done. "Upload files in parent directory with: cd ..; pio run -t uploadfs"

configImageFiles:

ifndef $(TARGET_MAC)
    $(info TARGET_MAC not set)
else
    $(info TARGET_MAC set)
endif

clean:
	rm -f source/*.o stje imageBuilder systemImage
	rm -f `find . | grep \~`

install:
	@mkdir -p /usr/share/stje
	@cp systemImage /usr/share/stje
	@cp stje /usr/bin
