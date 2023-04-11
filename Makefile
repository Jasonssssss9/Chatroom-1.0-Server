bin=server
src=server.cpp single.cpp
LD_FLAGS=-std=c++2a -lpthread
# LD_FLAGS=-std=c++11
cc=g++

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)

.PHONY:clean
clean:
	rm -f $(bin)