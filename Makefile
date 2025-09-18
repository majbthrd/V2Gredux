# *****************************************************************************
# * THIS FILE IS PROVIDED AS IS WITH NO WARRANTY OF ANY KIND, INCLUDING THE   *
# * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. *
# *****************************************************************************

CFLAGS = -g -static
CFLAGS += -I./OpenV2G/src/transport
CFLAGS += -I./OpenV2G/src/codec
CFLAGS += -DEXI_STREAM=BYTE_ARRAY
CFLAGS += -I./OpenV2G/src/test
CFLAGS += -I./OpenV2G/src/appHandshake/
CFLAGS += -I./OpenV2G/src/din/
CFLAGS += -I./OpenV2G/src/xmldsig/
CFLAGS += -I./OpenV2G/src/iso1/
CFLAGS += -I./OpenV2G/src/iso2/

#CCPREFIX = arm-linux-gnueabi-

all: redux

COMMON_DEP = Makefile parameters.h urandom.h

OPENV2G_OBJS = ./OpenV2G/src/appHandshake/appHandEXIDatatypesEncoder.o ./OpenV2G/src/appHandshake/appHandEXIDatatypesDecoder.o ./OpenV2G/src/appHandshake/appHandEXIDatatypes.o ./OpenV2G/src/codec/BitInputStream.o ./OpenV2G/src/codec/DecoderChannel.o ./OpenV2G/src/codec/EXIHeaderEncoder.o ./OpenV2G/src/codec/BitOutputStream.o ./OpenV2G/src/codec/ByteStream.o ./OpenV2G/src/codec/EXIHeaderDecoder.o ./OpenV2G/src/codec/MethodsBag.o ./OpenV2G/src/codec/EncoderChannel.o ./OpenV2G/src/iso1/iso1EXIDatatypesEncoder.o ./OpenV2G/src/iso1/iso1EXIDatatypes.o ./OpenV2G/src/iso1/iso1EXIDatatypesDecoder.o ./OpenV2G/src/din/dinEXIDatatypes.o ./OpenV2G/src/din/dinEXIDatatypesEncoder.o ./OpenV2G/src/din/dinEXIDatatypesDecoder.o ./OpenV2G/src/xmldsig/xmldsigEXIDatatypes.o ./OpenV2G/src/xmldsig/xmldsigEXIDatatypesDecoder.o ./OpenV2G/src/xmldsig/xmldsigEXIDatatypesEncoder.o ./OpenV2G/src/transport/v2gtp.o ./OpenV2G/src/iso2/iso2EXIDatatypesDecoder.o ./OpenV2G/src/iso2/iso2EXIDatatypes.o ./OpenV2G/src/iso2/iso2EXIDatatypesEncoder.o

REDUX_OBJS = redux.o $(OPENV2G_OBJS)

%.o: %.c $(COMMON_DEP)
	$(CCPREFIX)gcc $(CFLAGS) -c $< -o $@

redux: $(REDUX_OBJS) $(COMMON_DEP)
	$(CCPREFIX)gcc $(CFLAGS) $(REDUX_OBJS) -o $@
	$(CCPREFIX)strip $@

clean:
	rm -f redux
	rm -f $(REDUX_OBJS)

