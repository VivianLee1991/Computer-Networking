all: clean dht_peer dht_client

dht_peer: dht_peer.o mylib.o sha1.o
	gcc -o dht_peer dht_peer.o mylib.o sha1.o -std=gnu99 -lpthread

dht_peer.o: dht_peer.c
	gcc -c -o dht_peer.o dht_peer.c -std=gnu99 -lpthread

dht_client: dht_client.o mylib.o sha1.o
	gcc -o dht_client dht_client.o mylib.o sha1.o -std=gnu99

dht_client.o: dht_client.c
	gcc -c -o dht_client.o dht_client.c -std=gnu99

mylib.o: mylib.c
	gcc -c -o mylib.o mylib.c -std=gnu99

sha1.o: sha1.c
	gcc -c -o sha1.o sha1.c -std=gnu99

clean:
	rm -rf dht_peer dht_peer.o dht_client.o mylib.o sha1.o