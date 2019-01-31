CC = g++
PROM = client_fuse
SOURCE = client_fuse.c fs/gramfs_super.cc fs/gramfs.cc tools/logging.cc
$(PROM): $(SOURCE)
	$(CC) -std=c++11 `kcutilmgr conf -i` -o $(PROM) $(SOURCE) `kcutilmgr conf -l` `pkg-config fuse --cflags --libs`
