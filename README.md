
#      FPF - Frame Processing Framework


FPF is a flexible framework of objects that can be used to construct
different processing algorithms for frame organized data streams.

It was initially designed and build targeting processing of raw satellite
telemetry conforming CCSDS standards. Even more precisely,
it was intended to build simple tools to conduct an inventory, quality control
and initial processing steps of data downlinked from EOS missions (TERRA
and AQUA satellites).

FPF is free software.  You may copy, distribute, and modify it under
the terms of the License contained in the file LICENCE distributed
with this source code and binary packages.  This license is the same as 
the MIT/X Consortium license

## Installation

You may get either ready to run binaries or source code.

### Binary distribution

For each milestone release binary distribution
of the framework engine is provided for Linux-x64 and Windows operating
systems. The binary packages, packed as zIp or tar+gz archives, contain
a single engine executable file, a set of sample chain configuration
ini-files and a few documenting text files. Deployment of the binary release
is very trivial - just place the files anywhere you think it would be
convenient for you to find them. You may test if everything is OK and the
executable is compatible with your system simply running *fpf* command in
you terminal window.  Running without arguments it will print the version
line and then will complain that no configuration file is given.
Spend some time to look through this manual to get familiar with the system
and learn how you may control the engine and construct usable processing chains.

### Building from the source code

 FPF source code is hosted on the Github.
So you may always get access to the latest source code and documentation visiting
[Github project space](https://github.com/alxndrsh/fpf). To build the framework
you need GCC C++ compiler and GNU make utility (some other compilers may also
work without or with minimal adjustments).
The core of the framework and most of the node classes do not use anything
except the plain C++ 98. But for some classes additional external libraries,
specific language or compiler features may be required. If you have trouble with
such classes you may remove them from the project usually without consequences for other parts
(sure, at a price of loosing some functionality implemented by the dropped classes).

To build the source on Linux operating system open terminal window, step
to a folder where you want to keep the build tree and fetch the latest
source code from the Github with command

```
git clone https://github.com/alxndrsh/fpf.git
```

if you have no git tools installed you may download zipped source code
archive from the [Github page] (https://github.com/alxndrsh/fpf), unzip
it in any working folder. Step down to the *fpf* subfolder. Identify which
of the provided makefiles better suits your needs:

- *Makefile* - most generic makefile used for development builds on Linux/GCC.
   It has all the classes and all features switched on. Recommended for first try.

- *Makefile_nocurl* - Linux/GCC makefile configured for build without CURL library.
  ( USE_CURL is not defined). May be used if  your system has not libcurl installed
   or you have troubles linking with it. Without CURL some networking and online communication
   features of some processing nodes may be disabled.

- *Makefile_win* - - Makefile to cross-build FPF on Linux to get executable
  natively running on Windows operating system. To do such cross compilation
  [MinGW](http://www.mingw.org/) build environment should be installed.
  Check the makefile to be sure the MinGW tools are referenced from the proper locations.
 
Run the make utility using selected makefile version:
```
make -f Makefile
``` 
These makefiles build both debug and release(optimized) program versions
in bin/Debug and bin/Release subfolders. Release version is recommended for
generic use. Debug instrumentation may be helpful to diagnose some issues
in case of suspicious or erroneous program behavior.


## General framework structure

The framework is designed to support streamlined  processing of data
structured as a sequence of blocks. Such blocks may be called *frames*,
*packets*, *messages*, the only important and required condition are that
suck blocks should have finite size,  originate as a whole, and come into
processing chain strictly one after another. Through the full documentation and the code
such blocks are called _frames_ (even if for e.g. CCSDS "source packets" are
processed).

FPF provides a number of distinct objects that let one to solve different frame processing
tasks. Thanks to objects structure and following a few rules one may construct different
processing chains for different tasks or different processing scenarios. At the top level of
design each processing scenario should perform in cycle through three operations:
read input bit stream, detect and select frames, perform different tasks with the frames one by one.
To support such generic scheme FPF defines three main types of processing objects (actually implemented
as abstract C++ classes, all others should inherit from one of these base ones).

- *Framer* or *frame source* - the core of the framework, implementations of this class are responsible for "generation"
 of the frames. "Generation" means that such object do not receive frames at input but produces
them at the output. In most cases framers will have to read some input data to generate
frames. At the output of the framer frame is represented as a structure containing a pointer to the
frame data and, plus, some additional attributive information. The task of the framer is just to
identify the frames in the input data, format the frame structure content and pass it for further
processing.

- *InputStream* - generic class representing sequential unstructured bit stream. It works only for input
(reading) and only in strictly sequential mode. Such objects may represent and support reading data from
usual files, network sockets, hardware streaming devises, etc.  InputStream object may be used by
a framer to read input data.

- *Frame Processing Nodes* - classes that do actual frame processing. To provide promised
flexibility in building different processing scenario the nodes should be designed to perform
elementary operations on frames. To perform complex tasks the nodes may be connected in
_processing chains_.  Frames are passes from one node to the next one in the chain. So, each
processing node should take a frame from the previous node, do anything it is designed to
do with the frame, pass it to the next node. The node, doing its processing tasks, is free to perform different
operations. It may change content of the frame data or attributive information in the frame structure.
It may output frame data or subsets of the data to files or sockets. It may decide not to pass
some frames to the next node (thus a frame filter or block, depending on some conditions, may
be implemented as a node type).  It may output frames of different kind ( e.g. a node may
take CCSDS transfer frames as input and output CCSDS source packets at the output).
It may even have several output ports sending the same (or different selections of)
frames to further nodes thus forking initial chain into several branches.

To get better understanding what all these very generic words are about
and what you may accomplish with all this staff, we encourage you to
look further though the description of classes already implemented in
the framework and try some sample chains built to solve real world
tasks.

## Framework building blocks manual

The framework is a set of quite self consistent and independent classes. So the
main meaningful description of what the program using FPF objects should be read
from the description of the available classes.

### Framer (Frame Source) classes

#### FrameSource_CADU

*FrameSource_CADU* class reads a bit stream from a *InputStream*. Then it
identifies CCSDS transfer frames (CADU) based on known sync markers (ASM)
and formats frame structures to be passed further to the processing chain.
Object of this class is a core of the processing algorithms working with
SSCDS CADUs.

Class parameters:
 
- *input* [M] - mandatory parameter, this should be a name of InputStream
object in the configuration. This InputStream will be used to read input bit
stream.

- *sync marker* - sync marker identifying beginning of frame in the input bit
stream. Value should be written in hexadecimal notation and have exactly 4 bytes
(thus the ASM is 32 bit long). Default value for this parameter - standard
CCSDS ASM equal to 1ACFFC1D(hex)

- *frame_size* - size of the frames in bytes. This size includes sync markers, thus
this is also exact distance from the beginning of one ASM to the expected beginning
of the  ASM of the next packet. All frames in input stream should have
the same size. Default value = 1024 (this is the CADU size for TM stream from
AQUA and TERRA satellites)

- *fix_iq* - *yes/no* parameter. Setting this parameter to *yes* makes the
framer to try resolving bit coding ambiguities that may originate from
principal phase ambiguities in QPSK modulation.  Inversion of all the bits
in the input stream is also considered as one of the possible and resolved
ambiguity cases. Most modern demodulators resolves such ambiguities and
even perform frame synchronization in hardware, so "no" value may be used
in such cases. Set it to *yes* if you have doubts. Default value is *no*

- *buffer_size* - size of the input buffer (in bytes) to read data from the
input stream. Default value = 512 x times the given frame size (thus reading
about 512 frames per read operation).  The value may be given with "K" suffix
after a number, that means KiB (1024bytes) units. This parameter may be
accommodated to the input stream rate coming in real time.

#### FrameSource_PDS

*FrameSource_PDS* class reads a stream of CCSDS source packets and generates
FPF frame structures for each packet. So object of this class are required to
read and process packet level data. Such data level in NASA terminology used
for EOS (TERRA/AQUA) processing is called Level-0 (or PDS that stands for
Production Data Set). In contrast to the CADU frame stream the packets are not
required to have the same size. To coordinate their identification input packets
are required to have valid packet headers conforming CCSDS standard and valid
packet size field values inside this headers.

Class parameters:

- *valid_apids* - comma separated list of APIDs (in decimal notation) that are
considered to be valid in the input stream. Packets with other APIDs are ignored
and not passed to the further chain.

- *valid_sizes* -  comma separated list of valid packet sizes. Packets having
another value in the 'packet size' field will be ignored.

- *obt_epoch* - epoch ( 0 -time ) of the on-board time taken from the secondary
packet header and written into the attribute OBT timestamps of the frame
structure. Value should be given as a number of seconds between the actual OBT
epoch (0 time onboard time is referenced to) and "POSIX epoch" (1 January 1970).
Default value is epoch used on TERRA and AQUA satellites = -378691200
(that represents 01-Jan-1958).  OBT Epoch for Metop-A/B satellites
is 01-Jan-2000, thus processing data from these satellites for correct time stamps
one should specify this parameter explicitly ( *obt_epoch=946684800" ).

### Input Stream classes

#### InputStream_File

InputStream_File, as its name suggests, is used to read input bit stream from
files.

Parameters:

- *start_offset* - number of bytes to skip from the beginning of the file right
after opening it. This lets one to skip some unnecessary data at the beginning
or just read a part in the middle of the file. Default value = 0.

- *read_bytes* - number of bytes to read. As soon as the counter of read bytes
will reach this value the object closes the file and reports to the reader that
the end of file is reached. Using both  *start_offset*  and *read_bytes* one
may read any desired part of the file. If this parameter is not gives input file
will be read till the end.

- *real_time* - *yes/no* parameter making the object to operate in "real-time"
mode. This mode is intended to read data from a file which is not completely
written and still may grow at the end. In this mode the object does not
terminates reading when end-of-file condition is encountered, but makes a
pause and tries to read the input file again. If new data have been appended
to the end of file it will be consumed and passed to the reader. Thus such mode
may let one to run processing of the data coming in real time from e.g. receiver
or slow remote server not waiting while the full file will be written and
finalized. The object working in real-time mode with close the file and return
end-of-file to the read after some timeout expired when the size of the file
will stop growing. So if not intended to be used with growing in real time files
it is better to keep this parameter set to "no", otherwise some unnecessary
delay will be introduced at the end of file reading.

### Frame processing nodes

#### CNode_Counter

*CNode_Counter* is one of the simplest frame processing nodes. It does quite
trivial task - counts number of frames passing though. No changes are made to
the frame data or attributes. Recall that frame filters, blocking nodes
and routers may be present in the processing chain. So placing several counter
objects in different places may be reasonable to control how many frames passed
each control point. The total number of counted packets are reported to the
screen at the end of processing. Look at the reported name of the counter node
if several object have been instantiated

Parameters.

- *trace_every* - period of generation of tracing output of current counter
value. If this parameter value is N then the node will print to the screen
its name and current counter value at each Nth passing frame. Value 0 blocks
tracing output (but final counter value will be anyway reported at the end of
processing)

#### CNode_Descrambler* -

*CNode_Descrambler* performs descambilng (derandomisation) of the frame content.
This operation is performed as bitwise XOR of the frame data with given pseudorandom
bit sequence. 

Parameters:

- *pns_type* - type of pseudorandom sequence to use. The only support now value
is *ccsds_cadu* meaning the standard sequence defined in CCSDS TM 
synchronization and framing standard. Default value = *ccsds_cadu*

- *offset_in_frame* - offset to the first byte within the frame that should be
processed though the derandomization process. Usually ASM marker, which is kept
at the beginning of the frame data, should be skipped. Default value is 4.

- *descramble_bytes* - number of bytes in the frame data, starting from 
*offset_in_frame*, that should be derandomized. Default value = 1020
( with *offset_in_frame=4* lets one to descramble the full body, but not ASM,
 of 1024bytes frames, the case of AQUA/TERRA TM stream)
 
 
#### CNode_RS

*CNode_RS* node class perform block error detection and correction using
Reed-Solomon algorithm. Currently implemented algorithm type (225,223)
with interleaving x4.  At the end of processing the node reports total counters
of passed frames, frames with corrected errors, number of frames with
uncorrected errors.

Parameters:

- *fix_data* - *yes/no* parameter commanding the node to fix bit errors
discovered  in the frame data. If *no* value is given then the node just checks
and records into the frame attributes number of bit errors but does not modify
the frame data.

- *block_uncorrected* -  *yes/no* parameter. If set to *yes*, the node will
block frames that can not be corrected. This setting is recommended for normal
operation because in such case all the passed further frames will be correct and
thus interpreted with confidence.


#### CNode_PacketExtractor
*CNode_PacketExtractor* processing node performs extraction and reconstruction
of CCSDS source packets from the transfer frames (CADU). Pay attention, that
this node actually has two outputs. One is linked to the next node using
common *next_node* parameter. All input frames are passed to this output link
without modifications. This let one to continue processing frames in the chain
even after *CNode_PacketExtractor* node (for example, to stack several packet
extractors one after another to extract packets from different virtual channels).
The second output is used to emit the source packets (recall, they also become
*frames* in out terminology). Sizes of transfer frames and source packets may be
not synchronized (moreover, one stream may contain a mix of packets with
different sizes), so the packets are generated and passed via the second link
not at every input frame arrival.

Parameters:

- *vcid* [M] - identifier of the virtual channel  (VCID) for the packets to be
extracted from. Must be given as a single integer in decimal notation

- *valid_apids* - comma separated list of APIDs to select only corresponding
packet types.

- *valid_sizes* - comma separated list of packet sizes to select only corresponding
valid packets.

- *vcdu_insert* - size (in bytes) of the optional *insert zone* that may be
present in some types of packets. (e.g. for METOP insert zone has size =2, on
TERRA/AQUA insert zone is absent). Default value = 0.

- *packet_chain_node* - name of the frame processing node which should be connected to the
"second" output to receive generated source packets. This is a beginning of new
subchain branch.

- *obt_epoch* - epoch ( 0 -time ) of the on-board time taken from the secondary
packet header and written into the attribute OBT timestamps of the frame
structure. Value should be given as a number of seconds between the actual OBT
epoch (0 time onboard time is referenced to) and "POSIX epoch" (1 January 1970).
Default value is epoch used on TERRA and AQUA satellites = -378691200
(that represents 01-Jan-1958).  OBT Epoch for METOP-A/B satellites
is 01-Jan-2000, thus processing data from these satellites for correct time stamps
one should specify this parameter explicitly ( *obt_epoch=946684800" ).

 






 
