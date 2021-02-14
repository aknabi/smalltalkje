CC = gcc
LIBS = -lm
default:
	@cp ../src/*.c source
	@cp ../src/*.h source
	@cp source/build_image.h source/build.h
	@rm source/main.c
	@cd source && $(CC) -c memory.c names.c news.c interp.c primitive.c filein.c lex.c parser.c unixio.c st.c initial.c uart.c tty.c
	@cd source && $(CC) memory.o names.o news.o interp.o primitive.o filein.o lex.o parser.o unixio.o st.o uart.o tty.o -o ../lst3 $(LIBS)
	@cd source && $(CC) memory.o names.o news.o interp.o primitive.o filein.o lex.o parser.o unixio.o initial.o uart.o tty.o -o ../buildImage $(LIBS)
#	@cd bootstrap && ../buildImage basic.st mag.st collect.st file.st mult.st tty.st
#	@mv bootstrap/systemImage .
	./buildImage basic.st mag.st collect.st file.st mult.st oled.st m5StickC.st tty.st
#	@cp ./systemImage ../spiffs_image/systemImage
#	@cp ./objectTable ../spiffs_image/objectTable
#	@cp ./objectData ../spiffs_image/objectData
	@cp ./systemImage ../data/systemImage
	@cp ./objectTable ../data/objectTable
	@cp ./objectData ../data/objectData
	@cp ./objectData ../objects/objectData
	@echo Done. "Upload files in parent directory with: cd ..; pio run -t uploadfs"

image:
	./buildImage basic.st mag.st collect.st file.st mult.st oled.st m5StickC.st tty.st
	rm -f ../spiffs_image/systemImage
	@mv ./systemImage ../spiffs_image/systemImage

clean:
	rm -f source/*.o lst3 buildImage systemImage
	rm -f `find . | grep \~`

install:
	@mkdir -p /usr/share/lst3
	@cp systemImage /usr/share/lst3
	@cp lst3 /usr/bin
