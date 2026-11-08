all:tcp_srv tcp_cli
tcp_srv:tcp_srv.cc
	g++ -std=c++11 $^ -o $@
tcp_cli:tcp_cli.cc
	g++ -std=c++11 $^ -o $@

.PHONY:clean
clean:
	rm -rf tcp_srv tcp_cli