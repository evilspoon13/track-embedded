export CXX      = g++
export CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I../common -I/usr/local/include
export LDFLAGS  = -L/usr/local/lib -lrt -lpthread

# Pass PLATFORM=DRM for headless Pi, defaults to X11
export PLATFORM ?= X11

SUBDIRS = can-reader data-logger graphics-engine cloud-bridge

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

.PHONY: all clean $(SUBDIRS)
