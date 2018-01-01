all:  ext2_utils ext2_ls ext2_mkdir ext2_rm ext2_restore ext2_cp ext2_checker


ext2_ls: ext2_utils.o
	gcc -Wall -g $@.c -o $@ $^

ext2_mkdir: ext2_utils.o
	gcc -Wall -g $@.c -o $@ $^

ext2_rm: ext2_utils.o
	gcc -Wall -g $@.c -o $@ $^

ext2_restore: ext2_utils.o
	gcc -Wall -g $@.c -o $@ $^

ext2_cp: ext2_utils.o
	gcc -Wall -g $@.c -lm -o $@ $^

ext2_checker: ext2_utils.o
	gcc -Wall -g $@.c -o $@ $^

ext2_utils: 
	gcc -Wall -c $@.c $^

clean : 
	rm -f *.o ext2_utils ext2_ls ext2_mkdir ext2_rm ext2_restore ext2_cp ext2_checker