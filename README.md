Description:

The goal of this project was to write a low level distributed web application for file management. Application consists of server nodes and client nodes. Client and server nodes communicate with each other using a defined protocol. Nodes cooperate with each other in clusters. They can dynamically join and leave groups. Every node serves the same set of functionalities (server or client). Client nodes add interface for adding new files, downloading files, searching for files or removing files. Server nodes are responsible for storing files. 

Protocole description is in polish (10 pages too much to translate ;)):

Build:

With Cmake, or wih makefile. Both methods require global boost and ptthreads installation.
