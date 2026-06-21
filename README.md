# reverse-tunnel

## Building

```bash
cmake -B build
cmake --build build
```

## Running the client

Pass port mappings as `local:remote` pairs either via a file or directly on the command line.

**From a file** (one mapping per line):

```bash
./build/src/client/client -f examples/mappings.txt
```

**From the command line** (comma-separated):

```bash
./build/src/client/client -l "22:2222,80:8080,443:4443"
```
## Server-client handshake

on the initial connection between the client and the server the client will
send an initial message on port number <TBD the port that this uses> with the
following information:

Mapping count (2 byte unsigned int)
Mapping 1 from (2 byte unsigned int)
Mapping 1 to (2 byte unsigned int)
...
Mapping <mapping count> from (2 byte unsigned int)
Mapping <mapping count> to (2 byte unsigned int)
