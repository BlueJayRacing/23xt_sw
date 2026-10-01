CC      = clang
CFLAGS  = -std=c11 -Wall -Inanopb -Igenerated
NANOPB  = nanopb/pb_encode.c nanopb/pb_decode.c nanopb/pb_common.c
PROTOS  = generated/proto/channel_sample.pb.c

VENV            = venv
NANOPB_PROTO_DIR = $(VENV)/lib/python3.14/site-packages/nanopb/generator/proto

all: server client

server: server.c $(PROTOS)
	$(CC) $(CFLAGS) server.c $(PROTOS) $(NANOPB) -o server

client: client.c $(PROTOS)
	$(CC) $(CFLAGS) client.c $(PROTOS) $(NANOPB) -o client

gen:
	$(VENV)/bin/nanopb_generator -D generated proto/channel_sample.proto

gen-py:
	$(VENV)/bin/python -m grpc_tools.protoc -Iproto \
	  -I$(NANOPB_PROTO_DIR) --python_out=generated proto/channel_sample.proto
	$(VENV)/bin/python -m grpc_tools.protoc -I$(NANOPB_PROTO_DIR) \
	  --python_out=generated nanopb.proto

clean:
	rm -f server client

.PHONY: all gen gen-py clean
