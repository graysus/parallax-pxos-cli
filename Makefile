CXXFLAGS=
OUT=out/pxos
ALL_CXXFLAGS=$(CXXFLAGS) -Iinclude/ -I/usr/include/parallax/ -lparallax -lpxinternal -lblkid -lmount -lcurl
PREFIX?=/usr
DESTDIR?=/

all: $(OUT)

obj/:
	mkdir --parents obj/

obj/%.o: src/%.cpp obj/
	g++ $(ALL_CXXFLAGS) -c -o $@ $<

$(OUT): $(patsubst src/%.cpp,obj/%.o,$(wildcard src/*.cpp))
	mkdir --parents "out/"
	g++ -o $(OUT) $(ALL_CXXFLAGS) $^

clean:
	for i in obj out; do [ -e "$$i" ] && rm -rf "$$i"; done || true

install:
	mkdir --parents "$(DESTDIR)/$(PREFIX)/bin" "$(DESTDIR)/etc"
	install -m755 -o0 "$(OUT)" "$(DESTDIR)/$(PREFIX)/bin/pxos"
	install -m644 -o0 "support/pxos.conf" "$(DESTDIR)/etc/pxos.conf"