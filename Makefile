bin=server
src=server.cpp single.cpp protocol.cpp
LD_FLAGS=-std=c++2a -lpthread
# LD_FLAGS=-std=c++11
cc=g++

$(bin):$(src)
	$(cc) -o $@ $^ $(LD_FLAGS)
	mkdir message
	mkdir files

.PHONY:clean
clean:
	rm -f $(bin)
	rm -r message
	rm -r files