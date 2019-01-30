CC = g++
PROM = gramfs
SOURCE = client_fuse.c fs/gramfs_super.cc fs/gramfs.cc
$(PROM): $(SOURCE)
	$(CC) -std=c++11 `kcutilmgr conf -i` -o $(PROM) $(SOURCE) `kcutilmgr conf -l` `pkg-config fuse --cflags --libs`
