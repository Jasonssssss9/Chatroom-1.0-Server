bin=server
src=server.cpp single.cpp
LD_FLAGS=-std=c++17 -lpthread
cc=g++

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)

.PHONY:clean
clean:
	rm -f $(bin)