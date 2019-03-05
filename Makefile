CC=g++

all: my-router

my-router:
	$(CC) helper.cpp my-router.cpp -o my-router -lrt

clean:
	rm my-router routing-output*.txt