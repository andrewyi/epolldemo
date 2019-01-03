EXECUTABLE := proxy
TEST_EXECUTABLE := event_test

proxy: proxy.cpp
	g++ proxy.cpp -ggdb -o $(EXECUTABLE)

test: event_test.cpp
	g++ event_test.cpp -ggdb -o $(TEST_EXECUTABLE)

clean:
	rm $(EXECUTABLE) $(TEST_EXECUTABLE)

.PHONY: all clean test
