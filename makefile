all:LFS

run:
	./LFS
	
clean:
	rm -rf DRIVE
	rm -rf *.txt
	rm -rf LFS

LFS:checkpoint.cpp imap.cpp inode.cpp segment.cpp LFS.cpp
	g++ checkpoint.cpp imap.cpp inode.cpp segment.cpp LFS.cpp -o LFS
	