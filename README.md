# C-Engine

A nice and simple low-level engine for simple game development in C99.


## Building

The engine is being built using C99, OpenGL ES2, [SDL2](https://www.libsdl.org/)
and a few other great libraries.
Supported Platforms are Linux and the Browser (WebAssembly) – Windows and native
Android will be implemented eventually and should in theory work without too many
changes.

```bash
# Linux:
$ make

# WebAssembly:
$ make CC=emcc
```

Afterwards, run the game using `$ ./cengine` or `$ emrun cengine.html`, depending on your platform.

To serve the game as a Progressive Webapp, build using `CC=emcc` and copy `src/web/pwa/service-worker.js` in the same directory as `cengine.html`. The directory `src/web/pwa/` needs to be accessible.


### Hot reloading

On linux, the engine supports hot reloading scenes & code used in them.
This requires changing the `Makefile`, `src/engine.c` and then running:
```
# Build loadable DLL
$ make scenes

# Send SIGUSR1 to running cengine process.
$ killall -USR1 cengine  # (not tested)
```


### Server

```bash
# Build the server
$ make -f src/server/Makefile

# Run it on port 9123 (instead of 9124) as we use a reverse proxy.
$ ./server -p 9123
```

```nginx
server {
	listen 9124 ssl;
	server_name gameserver.example.com;
	root /mnt/file_server;

	location / {
		sendfile on;
		sendfile_max_chunk 64m;
		tcp_nopush on;

		proxy_pass http://localhost:9123;
		proxy_http_version 1.1;
		proxy_set_header Upgrade $http_upgrade;
		proxy_set_header Connection "upgrade";
		proxy_set_header Host $host;

	}

	ssl_certificate /path/to/fullchain.pem;
	ssl_certificate_key /path/to/privkey.pem;
}
```

