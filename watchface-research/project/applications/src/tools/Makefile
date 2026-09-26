TOOLS=	espeakdsp dogfeed nxloadmon flkill flmessage nxlight \
	nx_bat_monitor_console nx_bat_monitor watchface_update \
	power_button editor fonts flchoose watchface

all: $(TOOLS)

install: $(TOOLS)
	cp $(TOOLS) $(TOMDIST)/bin

clean:
	rm -f $(TOOLS)

%: %.c
	$(CC) -o $@ -I$(ROOT)/kernel/include $@.c -lnano-X

watchface: watchface.c
	$(CC) -o $@ -I$(ROOT)/kernel/include \
		-I$(ROOT)/build/microwin/src/include $< -lnano-X -lm

watchface_update: watchface_update.c
	$(CC) -O2 -Wall -Werror -o $@ $<

%: %.cxx
	$(CXX) -o $@ $@.cxx -lfltk -lfltk_images
