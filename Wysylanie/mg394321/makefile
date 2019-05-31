TARGET: netstore-server netstore-client

netstore-server:
	g++ -L/usr/bin -DBOOST_LOG_DYN_LINK -lboost_program_options -lboost_filesystem -lboost_log -lpthread -lboost_system -Wall -Wextra -O2 -std=c++17 -o netstore-server Server.cpp

netstore-client:
	g++ -L/usr/bin -DBOOST_LOG_DYN_LINK -lboost_program_options -lboost_filesystem -lboost_log -lpthread -lboost_system -Wall -Wextra -O2 -std=c++17 -o netstore-client Client.cpp


.PHONY: clean TARGET
clean:
	rm -f netstore-server netstore-client *.o *~ *.bak
