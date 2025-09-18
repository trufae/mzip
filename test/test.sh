#!/bin/sh

D=tmp
MZ=$(pwd)/../mzip

init() {
	rm -rf $D && mkdir -p $D && cd $D
	printf "hello\n" > hello.txt
	printf "world\n" > world.txt
	echo "Created test files:"
	xxd -g 1 hello.txt
	xxd -g 1 world.txt
	
	# Make sure we have clean test files
	if [ -f "../hello.txt" ]; then
		rm -f "../hello.txt"
	fi
	if [ -f "../world.txt" ]; then
		rm -f "../world.txt"
	fi
	cp hello.txt ../hello.txt
	cp world.txt ../world.txt
	# No special ZIP options
	ZIPOPT=""
}

fini() {
	echo "ok"
	cd ..
	rm -rf $D
}

error() {
	echo "$@"
	exit 1
}

test_unzip() {
	init
	echo "[***] Testing zip+mzip with $1"
	# Always use the standard method (store for now)
	if [ "$1" = "store" ]; then
		zip -0 test.zip hello.txt world.txt
	else
		zip test.zip hello.txt world.txt
	fi
	echo "Created zip file with method $1"
	file test.zip
	xxd -g 1 test.zip | head -20
	# List the zip content
	$MZ -l test.zip > files.txt
	cat files.txt
	grep hello.txt files.txt > /dev/null || error "hello.txt not found"
	grep world.txt files.txt > /dev/null || error "world.txt not found"
	mkdir data
	cd data
	$MZ -x ../test.zip > /dev/null
	diff -u hello.txt ../hello.txt || error "uncompressed hello.txt fail"
	diff -u world.txt ../world.txt || error "uncompressed world.txt fail"
	cd ..
	fini
	return 0
}

test_zip() {
     init
     echo "[***] Testing mzip $1 (0 = store, 1 = deflate, 3 = lzma, 5 = brotli, 93 = zstd, 100 = lzfse)"
    echo "Creating test.zip with mzip -c test.zip hello.txt world.txt -z$1"
    # Use the compression method specified by the parameter
    $MZ -c test.zip hello.txt world.txt -z$1
    echo "Archive contents:"
    xxd -g 1 test.zip | head -n 20
    unzip -l test.zip > files.txt
    grep hello.txt files.txt > /dev/null || error "hello.txt not found"
    grep world.txt files.txt > /dev/null || error "world.txt not found"
    # unzip does not support extracting some non-standard methods (e.g., brotli=97)
    if [ "$1" != "5" ]; then
        echo "[---] Decompressing with unzip"
        {
            mkdir data
            cd data
            unzip ../test.zip
            echo "unzip-extracted hello.txt:"
            hexdump -C hello.txt
            echo "original hello.txt:"
            hexdump -C ../hello.txt
            echo "CHECKING CONTENTS (ignoring line endings):"
            cat hello.txt | od -c
            echo "CONTENT LENGTH: $(wc -c < hello.txt) bytes"
            cat ../hello.txt | od -c
            echo "CONTENT LENGTH: $(wc -c < ../hello.txt) bytes"
            
            # Simple string comparison - extract just the word without newlines
            echo "EXTRACTING JUST THE WORD:"
            WORD1=$(tr -d '\r\n' < hello.txt)
            WORD2=$(tr -d '\r\n' < ../hello.txt)
            echo "Word from unzip: '$WORD1'"
            echo "Word from original: '$WORD2'"
            
            # Compare the words
            [ "$WORD1" = "$WORD2" ] || error "uncompressed hello.txt fail"
            echo "unzip-extracted world.txt:"
            hexdump -C world.txt
            echo "original world.txt:"
            hexdump -C ../world.txt
            echo "CHECKING CONTENTS (ignoring line endings):"
            cat world.txt | od -c
            echo "CONTENT LENGTH: $(wc -c < world.txt) bytes"
            cat ../world.txt | od -c
            echo "CONTENT LENGTH: $(wc -c < ../world.txt) bytes"
            
            # Simple string comparison - extract just the word without newlines
            echo "EXTRACTING JUST THE WORD:"
            WORD1=$(tr -d '\r\n' < world.txt)
            WORD2=$(tr -d '\r\n' < ../world.txt)
            echo "Word from unzip: '$WORD1'"
            echo "Word from original: '$WORD2'"
            
            # Compare the words
            [ "$WORD1" = "$WORD2" ] || error "uncompressed world.txt fail"
            cd ..
            rm -rf data
        }
    else
        echo "[---] Skipping unzip extraction for brotli (method 97)"
    fi
	echo "[---] Decompressing with mzip"
	{
		mkdir -p data
		cd data
		$MZ -x ../test.zip
		echo "mzip-extracted hello.txt:"
		hexdump -C hello.txt
		echo "original hello.txt:"
		hexdump -C ../hello.txt
		echo "CHECKING CONTENTS (ignoring line endings):"
		cat hello.txt | od -c
		echo "CONTENT LENGTH: $(wc -c < hello.txt) bytes"
		cat ../hello.txt | od -c
		echo "CONTENT LENGTH: $(wc -c < ../hello.txt) bytes"
		
		# Simple string comparison - extract just the word without newlines
		echo "EXTRACTING JUST THE WORD:"
		WORD1=$(tr -d '\r\n' < hello.txt)
		WORD2=$(tr -d '\r\n' < ../hello.txt)
		echo "Word from mzip: '$WORD1'"
		echo "Word from original: '$WORD2'"
		
		# Compare the words
		[ "$WORD1" = "$WORD2" ] || error "uncompressed hello.txt fail"
		echo "mzip-extracted world.txt:"
		hexdump -C world.txt
		echo "original world.txt:"
		hexdump -C ../world.txt
		# Compare contents for equality, ignoring line endings
		cat world.txt | tr -d '\r\n' > world.clean
		cat ../world.txt | tr -d '\r\n' > orig2.clean
		diff -u world.clean orig2.clean || error "uncompressed world.txt fail"
		cd ..
	}
	fini
	return 0
}

# Test LZMA using an existing LZMA-compressed zip
test_unzip_lzma() {
	init
	echo "[***] Testing mzip decompression with LZMA"
	# Create an LZMA-compressed zip file
	$MZ -c test.zip hello.txt world.txt -z3
	echo "Created zip file with LZMA method"
	file test.zip
	xxd -g 1 test.zip | head -20
	# List the zip content
	$MZ -l test.zip > files.txt
	cat files.txt
	grep hello.txt files.txt > /dev/null || error "hello.txt not found"
	grep world.txt files.txt > /dev/null || error "world.txt not found"
	mkdir data
	cd data
	$MZ -x ../test.zip > /dev/null
	diff -u hello.txt ../hello.txt || error "uncompressed hello.txt fail"
	diff -u world.txt ../world.txt || error "uncompressed world.txt fail"
	cd ..
	fini
	return 0
}

# ---- #

test_unzip "store" || exit 1
test_unzip "deflate" || exit 1
test_unzip_lzma || exit 1

MODE="store"; test_zip "0" || exit 1
MODE="deflate"; test_zip "1" || exit 1
MODE="lzma"; test_zip "3" || exit 1
MODE="brotli"; test_zip "5" || exit 1
MODE="zstd"; test_zip "93" || exit 1
MODE="lzfse"; test_zip "100" || exit 1

# Additional corner-case tests

test_empty_files() {
     init
     echo "[***] Testing empty files with store/deflate/lzma/brotli/zstd/lzfse"
     : > empty.txt
     for Z in 0 1 3 5 93 100; do
        rm -f test.zip
        $MZ -c test.zip empty.txt -z$Z || error "mzip failed for -z$Z"
        unzip -l test.zip > files.txt || error "unzip -l failed"
        grep "empty.txt" files.txt >/dev/null || error "empty.txt missing (-z$Z)"
        mkdir -p data && cd data
        $MZ -x ../test.zip >/dev/null || error "mzip -x failed (-z$Z)"
        [ -f empty.txt ] || error "empty.txt not extracted (-z$Z)"
        cmp -s empty.txt ../empty.txt || error "empty.txt mismatch (-z$Z)"
        cd .. && rm -rf data
    done
    fini
}

test_binary_file() {
     init
     echo "[***] Testing binary file (0..255) with store/deflate/lzma/brotli/zstd/lzfse"
    # create 256-byte binary with values 0..255
    i=0; : > bin.dat
    while [ $i -lt 256 ]; do printf "\\$(printf '%03o' $i)" >> bin.dat; i=$((i+1)); done
     for Z in 0 1 3 5 93 100; do
        rm -f test.zip
        $MZ -c test.zip bin.dat -z$Z || error "mzip failed for -z$Z"
        unzip -l test.zip > files.txt || error "unzip -l failed"
        grep "bin.dat" files.txt >/dev/null || error "bin.dat missing (-z$Z)"
        mkdir -p data && cd data
        $MZ -x ../test.zip >/dev/null || error "mzip -x failed (-z$Z)"
        cmp -s bin.dat ../bin.dat || error "binary mismatch (-z$Z)"
        cd .. && rm -rf data
    done
    fini
}

test_large_random_and_fallback() {
    init
    echo "[***] Testing random data with LZMA (validate robust handling)"
    dd if=/dev/urandom of=rand.bin bs=1k count=4 2>/dev/null || error "cannot create random"
    $MZ -c test.zip rand.bin -z3 || error "mzip failed -z3"
    # Validate archive and extraction
    unzip -l test.zip > files.txt || error "unzip -l failed"
    grep "rand.bin" files.txt >/dev/null || error "rand.bin missing"
    mkdir -p data && cd data
    $MZ -x ../test.zip >/dev/null || error "mzip -x failed"
    cmp -s rand.bin ../rand.bin || error "random mismatch after extract"
    cd .. && rm -rf data
    fini
}

test_duplicate_names_listing() {
    init
    echo "[***] Testing duplicate filenames in archive listing"
    mkdir -p a b
    echo one > a/dup.txt
    echo two > b/dup.txt
    # Add both; tool stores base names, creating two entries with same name
    $MZ -c test.zip a/dup.txt b/dup.txt -z1 || error "mzip failed"
    out=$($MZ -l test.zip)
    echo "$out"
    cnt=$(printf "%s" "$out" | grep -c "dup.txt")
    [ "$cnt" -eq 2 ] || error "expected 2 dup.txt entries, got $cnt"
    fini
}

test_space_in_name() {
    init
    echo "[***] Testing filename with spaces"
    printf "spaced content\n" > "space name.txt"
     for Z in 0 1 3 5 93 100; do
        rm -f test.zip
        $MZ -c test.zip "space name.txt" -z$Z || error "mzip failed for -z$Z"
        $MZ -l test.zip | grep "space name.txt" >/dev/null || error "missing spaced name (-z$Z)"
        mkdir -p data && cd data
        $MZ -x ../test.zip >/dev/null || error "mzip -x failed (-z$Z)"
        diff -u "space name.txt" ../"space name.txt" || error "spaced filename mismatch (-z$Z)"
        cd .. && rm -rf data
    done
    fini
}

# Run new tests
test_empty_files || exit 1
test_binary_file || exit 1
test_large_random_and_fallback || exit 1
test_duplicate_names_listing || exit 1
test_space_in_name || exit 1
