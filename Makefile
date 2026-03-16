run: hts
	./hts input.hts

hts: main.c
	cc -O0 -g $< -o $@

.PHONY: run
