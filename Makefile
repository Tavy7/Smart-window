window: window_example.cpp
	g++ $< -o $@ -lpistache -lcrypto -lssl -lpthread -std=c++17
	