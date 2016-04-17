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

1. When creating new connections through the irc_connect command the callbacks
to listen for data are not assigned correctly (as they are for the primary connection).
2. As a result of #1. multi-connections are not really being handled properly right now
3. Non-standard build process. The code should be using autotools and also we need
to annotate all the dependencies needed by the built-in perl modules, this should
either be an automated process during install or at least list what is needed, automatically.

## Config design

The configuratoin is stored as a perl hash. As such it we want it to be live
editible. But currently it is not. The goals of the config should be as follows:

* Changes to config trigger a "config_change" event which can be hooked to in modules
* Fully automated config change response. Meaning if you remove a channel, the bot parts
that channel live. If you add a channel it joins, etc.. if you remove a server from the bot
it disconnects from that server and so on. NOTE: This is going to be difficult because we have 
to fix a lot of bugs with the server re-connection/connecting from perl code which is currently
being worked on. We also desperately need to fix the child handling which I'm also currently working on.
* 
