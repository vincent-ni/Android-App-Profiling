
CC=g++
AR=ar
NVCC=nvcc

ifdef DEBUG_MODE
DFLAGS=-g
else
DFLAGS= -O3 -DNDEBUG
endif

CFLAGS=$(DFLAGS) -Wall -fasm-blocks
NVCCFLAGS=$(DFLAGS)

LDFLAGS=rc

OPENCV_INCLUDE=/opt/local/include/opencv
BOOST_INCLUDE=/opt/local/include

OPENCV_LIB=/opt/local/lib
OPENCV_LIBS=-lcv -lcxcore -lhighgui

BUILD_PATH=../build

CUDA_INCLUDE=/Developer/CUDA/common/inc
CUDA_LIB=-L/Developer/CUDA/lib -L/usr/local/cuda/lib
CUDA_LIBS=-lcutil -lcudart

SOURCES_CPP=Example.cpp
SOURCES_CU=CudaCuts.cu
SOURCES=$(SOURCES_CPP) $(SOURCES_CU)

OBJECTS += $(patsubst %.cu,$(BUILD_PATH)/%.cu.o,$(SOURCES_CU))
OBJECTS += $(patsubst %.cpp,$(BUILD_PATH)/%.o,$(SOURCES_CPP))

EXECUTABLE=graphcut

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJECTS) \
	-L$(OPENCV_LIB) $(OPENCV_LIBS) $(CUDA_LIB) $(CUDA_LIBS)
#	cd $(BUILD_PATH); $(AR) $(LDFLAGS) $@ $(OBJECTS)
#	ranlib $(BUILD_PATH)/$@

$(BUILD_PATH)/%.o : %.cpp
	$(CC) $(CFLAGS) -I$(OPENCV_INCLUDE) -I$(BOOST_INCLUDE) -c $< -o $(BUILD_PATH)/$@

$(BUILD_PATH)/%.cu.o : %.cu
	$(NVCC) $(NVCCFLAGS) -I$(CUDA_INCLUDE) -c $< -o $(BUILD_PATH)/$@
	
clean:
	rm $(OBJECTS) $(EXECUTABLE)
	
