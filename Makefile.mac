NDKDIR=/Applications/Nuke6.0v3-32/Nuke6.0v3.app/Contents/MacOS

TARGET=SyLens.dylib
HEADERS=$(wildcard *.h)
SOURCES=$(wildcard *.cpp)
OBJECTS=$(SOURCES:.cpp=.o)

SDKROOT=/Developer/SDKs/MacOSX10.4u.sdk

CXXFLAGS+= -c -fPIC -DUSE_GLEW -I$(NDKDIR)/include
LNFLAGS+=-undefined warning -flat_namespace  -dynamiclib -shared -rdynamic -nodefaultlibs -m32 -ggdb

all: $(TARGET)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<
	
$(TARGET): $(OBJECTS)
	$(CXX) -o $(TARGET) $(LNFLAGS) $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(TARGET)
