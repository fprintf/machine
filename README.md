# machine
Purpose of this project is to create an extremely high performance multi-server scriptable IRC bot. The main goals are handling thousands of connections at once.

## Goals

* High performance
* Modular design
* Extensible via perl commands/modules/event hooks
* Multi-server (should be able to handle THOUSANDS of server connections)

## Current Features

* Asynchronous multi-process design
* Connections are handled asynchronously by a master process. This process
then dispatches out data coming in from the connections to the child processes. Each child process
is running it's own perl interpreter. The child process parses the message from the
connection and dispatches it to the perl interpreter running it's own event handlers.
* Perl handles all the commands and most events. The connection process
and specifying servers are still handled in the core code and need to be scriptable
which they are not right now.
* Adding new connections is possible but it's not handled correctly right now.

## Major issues needing to be addressed

O. When creating new connections through the irc_connect command the callbacks
to listen for data are not assigned correctly (as they are for the primary connection).
O. As a result of 1. multi-connections are not really being handled properly right now
O. I need to have a shared data "blackboard" between perl processes (this is easy with IPC::Shareable)
O. I need to address the config problem. The current design does not have a good way to read nested values in
the config, specifically nested hashes which is a major problem as the config is much easier to read if we
can do that. 
