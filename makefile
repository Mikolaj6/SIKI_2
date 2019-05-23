TARGET: netstore-server netstore-client

netstore-server:
	g++ -L/usr/bin -lboost_program_options -lboost_filesystem -lboost_system -o netstore-server Server.cpp

netstore-client:
	g++ -L/usr/bin -lboost_program_options -lboost_filesystem -lboost_system -o netstore-client Client.cpp


.PHONY: clean TARGET
clean:
	rm -f netstore-server netstore-client *.o *~ *.bak
