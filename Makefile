all: backup backup_restore precompressed build benchmark rzip coverage backup backup_restore

build:
	gcc rzip.c -o rzip.o
	gcc rzippc.c -o rzippc.o
	gcc -pg -fprofile-arcs -ftest-coverage -g -o rzipc.o rzip.c
	 

benchmark:
	@echo "RZIP: Build time"
	time gcc rzip.c -o rzip.o
	@echo "RZIP: Default config (cypher key on, no cypher string)"
	time ./rzip.o zip benchmark.rzip /usr/bin/ --silent --debug
	@echo "RZIP: Cypher string config (no cypher key on, cypher string replaces it)"
	time ./rzip.o zip benchmark.rzip /usr/bin/ --silent --cypher-string  --debug
	@echo "RZIP: Performance mode, no cypher key config (no cypher key on, cypher string replaces it)"
	time ./rzip.o zip benchmark.rzip /usr/bin/ --silent --no-cypher-key  --debug
	@echo "TAR : tar -cf with default settings"
	time tar -cf benchmark.tar /usr/bin 
	@echo "ZIP : zip with default settings"
	time zip -r -y -q benchmark.zip /usr/bin 
	du benchmark.rzip -h 
	du benchmark.tar -h
	du benchmark.zip -h
	rm benchmark.rzip
	rm benchmark.tar
	rm benchmark.zip



rzip: 
	@rm -rf first.rzip  2>/dev/null
	@rm -rf dir2  2>/dev/null
	gcc rzip.c -o rzip.o
	./rzip.o zip mybackup.rzip /usr/bin/ /home/retoor/Downloads/
	./rzip.o unzip mybackup.rzip mybackup/

test:
	gcc rzip.c -o rzip.o
	./rzip.o test --debug

install:
	@echo "This command requires sudo."
	gcc rzip.c -o rzip.o
	sudo cp rzip.o /usr/bin/rzip
	@echo "Installed. You can now execute rzip. See Readme.md for usage."

coverage: 
	@rm -f *.gcda   2>/dev/null
	@rm -f *.gcno   2>/dev/null
	@rm -f rzipc.coverage.info   2>/dev/null
	@rm -f rzipc.coverage.info   2>/dev/null
	gcc -pg -fprofile-arcs -ftest-coverage -g -o rzipc.o rzip.c
	./rzipc.o test --debug
	-@./zripc.o zip testdata.rzip testdata.non-existing.path
	./rzipc.o zip testdata.rzip testdata --silent 
	./rzipc.o unzip testdata.rzip testdata --silent
	./rzipc.o zip testdata.rzip testdata --silent  --no-cypher-key --debug 
	./rzipc.o unzip testdata.rzip testdata --silent --no-cypher-key --debug 
	./rzipc.o zip testdata.rzip testdata --silent  --cypher-string 
	./rzipc.o unzip testdata.rzip testdata --silent --cypher-string
	./rzipc.o list testdata.rzip --cypher-string
	-@./rzipc.o list testdata.non-existing.rzip
	./rzipc.o view testdata.rzip --cypher-string
	-@./rzipc.o view testdata.non-existing.rzip
	./rzipc.o help
	@./rzipc.o zip --zip-does-not-care-if-non-existing-does-nothing
	@./rzipc.o unzip --not-enough-params-test
	@./rzipc.o unzip non-existing-file none-existing-path --invalid-params-test
	./rzipc.o non-existing-command-to-trigger-default-switch
	lcov --capture --directory . --output-file rzipc.coverage.info
	genhtml rzipc.coverage.info --output-directory rzipc.coverage
	@rm -f *.gcda   2>/dev/null
	@rm -f *.gcno   2>/dev/null
	@rm -f rzipc.coverage.info   2>/dev/null
	@rm -f gmon.out
	google-chrome rzipc.coverage/index.html

precompressed:
	gcc -E -P -C rzip.c -o rzippc.c
	clang-format -i rzippc.c
