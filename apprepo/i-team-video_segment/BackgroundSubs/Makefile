CC=g++

ifdef DEBUG_MODE
DFLAGS=-g
else
DFLAGS=-O2
endif

CFLAGS=$(DFLAGS) -Wall -pthread

OPENCV_LIB=/opt/local/lib
OPENCV_LIBS=-lcv -lcxcore -lhighgui
OPENCV_INCLUDE=/opt/local/include/opencv

SOURCES=AviReader.cpp cvCodeBook.cpp test3.cpp
OBJECTS=$(SOURCES:.cpp=.o)

EXECUTABLE=backsubs

all : $(OBJECTS) $(EXECUTABLE)

$(EXECUTABLE) : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ \
	-L$(OPENCV_LIB) $(OPENCV_LIBS) -lpthread

%.o : %.cpp %.h
	$(CC) $(CFLAGS) -c $< -o $@ -I$(OPENCV_INCLUDE)

clean:
	rm -rf  $(OBJECTS) $(EXECUTABLE)
