build:
	g++ -std=c++17 -o ../bin/hearty-store-init hearty-store-init.cpp
	g++ -std=c++17 -o ../bin/hearty-store-put hearty-store-put.cpp
	g++ -std=c++17 -o ../bin/hearty-store-get hearty-store-get.cpp
	g++ -std=c++17 -o ../bin/hearty-store-list hearty-store-list.cpp
	g++ -std=c++17 -o ../bin/hearty-store-destroy hearty-store-destroy.cpp
	g++ -std=c++17 -o ../bin/hearty-store-replicate hearty-store-replicate.cpp
	g++ -std=c++17 -o ../bin/hearty-store-ha hearty-store-ha.cpp

clean:
	-rm -rf ../bin/*
	-rm -rf /tmp/store*
	-rm -rf /tmp/ha_group_*