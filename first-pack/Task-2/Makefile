SHELL := /bin/bash

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -fsanitize=address -fsanitize=leak
BIN = filetool

.PHONY: all clean test test-xor test-mask test-copy test-find \
        xt-xor-a xt-xor-b xt-xor-c xt-xor-d \
        xt-mask-a xt-mask-b xt-mask-c xt-mask-d xt-mask-e \
        xt-copy-a xt-copy-b xt-copy-c xt-copy-d \
        xt-find-a xt-find-b xt-find-c xt-find-d xt-find-e

all: $(BIN)

$(BIN): main.c
	$(CC) $(CFLAGS) main.c -o $(BIN)

clean:
	rm -f $(BIN) *.o *.out \
	      f*.bin g*.bin a*.bin b*.bin tail.bin zeros.bin all.bin big.bin \
	      sample.bin sample.bin1 sample.bin2 sample.bin3 \
	      a.txt b.txt empty.bin empty.bin1 big.bin1 empty.bin2 \
	      t*.txt list*.txt out.txt


test: $(BIN) test-xor test-mask test-copy test-find
	@echo "ВСЕ ТЕСТЫ ПРОЙДЕНЫ"

test-xor: xt-xor-a xt-xor-b xt-xor-c xt-xor-d
	@echo "[XOR] OK"

# A: единый поток, блок 2 байта (xor4), граница между файлами
xt-xor-a: $(BIN)
	@set -e; echo "[xor A] xor4, crossing boundary"
	printf '\xAA' > f1.bin             # [AA]
	printf '\xBB\xCC' > f2.bin         # [BB CC]
	./$(BIN) f1.bin f2.bin xor4 > out.txt
	grep -qx '66BB' out.txt

# B: xor2 (нибблы)
xt-xor-b: $(BIN)
	@set -e; echo "[xor B] xor2 nibble test"
	printf '\xAB' > f3.bin             # A^B = 1
	./$(BIN) f3.bin xor2 > out.txt
	grep -qx '1' out.txt

# C: xor3 (байты), «ABC»
xt-xor-c: $(BIN)
	@set -e; echo "[xor C] xor3 bytes"
	printf '\x41\x42\x43' > f4.bin     # 41^42^43=40
	./$(BIN) f4.bin xor3 > out.txt
	grep -qx '40' out.txt

# D: ровные блоки 2 байта *2 блока
xt-xor-d: $(BIN)
	@set -e; echo "[xor D] xor4 two full blocks"
	printf '\x01\x02' > g1.bin
	printf '\x03\x04' > g2.bin
	./$(BIN) g1.bin g2.bin xor4 > out.txt
	grep -qx '0206' out.txt

test-mask: xt-mask-a xt-mask-b xt-mask-c xt-mask-d xt-mask-e
	@echo "[MASK] OK"

# A: базовый — 0xF, 4 значения
xt-mask-a: $(BIN)
	@set -e; echo "[mask A] basic count"
	printf '\x00\x00\x00\x0F\xFF\x00\x00\x0F\x00\x00\x0F\x00\x00\x00\x00\x0F' > g.bin
	./$(BIN) g.bin mask 0xF > out.txt
	grep -qx '3' out.txt

# B: число собирается из двух файлов (12 34 | 56 78 -> 0x12345678)
xt-mask-b: $(BIN)
	@set -e; echo "[mask B] cross-file big-endian u32"
	printf '\x12\x34' > a1.bin
	printf '\x56\x78' > a2.bin
	./$(BIN) a1.bin a2.bin mask 0x12340000 > out.txt
	grep -qx '1' out.txt

# C: хвост < 4 байт игнорируем
xt-mask-c: $(BIN)
	@set -e; echo "[mask C] tail ignored"
	printf '\x11\x22\x33\x44\x55' > tail.bin
	./$(BIN) tail.bin mask 0x11223300 > out.txt
	grep -qx '1' out.txt

# D: нулевая маска (совпадает со всеми)
xt-mask-d: $(BIN)
	@set -e; echo "[mask D] zero mask"
	printf '\x01\x02\x03\x04\xAA\xBB\xCC\xDD' > zeros.bin
	./$(BIN) zeros.bin mask 0x0 > out.txt
	grep -qx '2' out.txt

# E: полная маска (только FFFFFFFF)
xt-mask-e: $(BIN)
	@set -e; echo "[mask E] full mask"
	printf '\xFF\xFF\xFF\xFF\x00\x00\x00\x00' > all.bin
	./$(BIN) all.bin mask FFFFFFFF > out.txt
	grep -qx '1' out.txt

# ---------- COPY ----------
test-copy: xt-copy-a xt-copy-b xt-copy-c xt-copy-d
	@echo "[COPY] OK"

# A: 3 копии + сравнение
xt-copy-a: $(BIN)
	@set -e; echo "[copy A] 3 copies and compare"
	printf '\xD
	E\xAD\xBE\xEF\x01\x02\x03\x04' > sample.bin
	./$(BIN) sample.bin copy3
	cmp -s sample.bin sample.bin1
	cmp -s sample.bin sample.bin2
	cmp -s sample.bin sample.bin3

# B: перезапись существующей копии
xt-copy-b: $(BIN)
	@set -e; echo "[copy B] overwrite existing copy"
	echo "old" > sample.bin1
	./$(BIN) sample.bin copy1
	cmp -s sample.bin sample.bin1

# C: пустой файл
xt-copy-c: $(BIN)
	@set -e; echo "[copy C] empty file"
	: > empty.bin
	./$(BIN) empty.bin copy2
	[ -f empty.bin1 ] && [ ! -s empty.bin1 ]
	[ -f empty.bin2 ] && [ ! -s empty.bin2 ]

# D: «большой» файл (1 МБ нулей), чтобы проверить потоковое копирование
xt-copy-d: $(BIN)
	@set -e; echo "[copy D] big file (1MB zeros)"
	dd if=/dev/zero of=big.bin bs=1 count=1048576 status=none
	./$(BIN) big.bin copy1
	cmp -s big.bin big.bin1

test-find: xt-find-a xt-find-b xt-find-c xt-find-d
	@echo "[FIND] OK"

# A: базовый — есть в t1 и t2, нет в list2
xt-find-a: $(BIN)
	@set -e; echo "[find A] basic"
	printf 'hello world\nalpha beta\ngamma\n' > t1.txt
	printf 'foo bar\nbaz\nworldwide web\n'    > t2.txt
	printf 'no hits here\n'                    > t3.txt
	printf 't1.txt\nt2.txt\n' > list1.txt
	printf 't3.txt\n'         > list2.txt
	./$(BIN) list1.txt list2.txt find world > out.txt
	grep -qx 't1.txt' out.txt
	grep -qx 't2.txt' out.txt
	grep -q '*Stirng not found in files*' out.txt

# B: чувствительность к регистру (искомое 'World' — не найдёт)
xt-find-b: $(BIN)
	@set -e; echo "[find B] case-sensitive"
	./$(BIN) list1.txt list2.txt find World > out.txt
	! grep -qx 't1.txt' out.txt
	! grep -qx 't2.txt' out.txt
	grep -q '*Stirng not found in files*' out.txt
	grep -q '*Stirng not found in files*' out.txt

# C: список с CRLF (Windows line endings)
xt-find-c: $(BIN)
	@set -e; echo "[find C] CRLF list"
	printf 't1.txt\r\n' > list1.txt
	printf 't2.txt\r\n' >> list1.txt
	./$(BIN) list1.txt find world > out.txt
	grep -qx 't1.txt' out.txt
	grep -qx 't2.txt' out.txt

# D: отсутствующие файлы в списке — должен просто не напечатать их, но не падать
xt-find-d: $(BIN)
	@set -e; echo "[find D] missing file in list"
	printf 'no_such.txt\n' > list1.txt
	./$(BIN) list1.txt find world > out.txt
	grep -q '*Stirng not found in files*' out.txt
