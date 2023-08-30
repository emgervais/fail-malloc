default: big_alloc

big_alloc: big_alloc.c
	gcc -g $^ -o $@

restrict: memrestrict.c
	gcc -g -shared -fPIC -ldl memrestrict.c -o libmemrestrict.so

restrict2: restrict.c
	gcc -g -shared -fPIC -ldl restrict.c -o libmemrestrict.so

clean:
	rm -f *.o big_alloc big_alloc_linker libmemrestrict.so ptrace-restrict
