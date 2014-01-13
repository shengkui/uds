 A Unix domain socket example
==============================
**Author**: Shengkui Leng

**E-mail**: lengshengkui@gmail.com


Description
-----------
This project is a demo for how to use Unix domain socket(Local socket) to
communicate between client and server.  And the server supports multiple
clients via multi-threading.

NOTES: There is no message boundaries in Unix domain socket with "SOCK\_STREAM"
type. Since Linux kernel v2.6.4, it supports "SOCK\_SEQPACKET", a
connection-oriented socket that preserves message boundaries and delivers
messages in the order that they were sent. Try "man unix" for more info about
it.

* * *

Build
-----------
(1) Open a terminal.

(2) chdir to the source code directory.

(3) Run "make"


Run
-----------
(1) Start the server:

>    $ ./server

(2) Start the client to send request to server:

>    $ ./client

