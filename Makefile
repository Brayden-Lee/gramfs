CC = g++
PROM = gramfs
SOURCE = client_fuse.c fs/gramfs_super.cc fs/gramfs.cc
$(PROM): $(SOURCE)
	$(CC) -std=c++11 -Wall -g `kcutilmgr conf -i` -lm -o $(PROM) $(SOURCE) `pkg-config fuse --cflags --libs`
