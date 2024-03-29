libHeracles
===========

A lens parser C library. Forked from [Augeas](http://augeas.net) to allow 
direct access to the parser functions.

It is built with the idea in mind to make a python augeas lens parser files
that is implemented in the [heracles project](https://github.com/llou/heracles),
all tests are built in it and the best way to know how it works is in it's code.

Status
------
libheracles is now in **alpha**, so you can play with it but it's use in production
is completely discouraged.


How it works
------------
The library is mainly focused to be the foundations of an implementation
in another language so it only provides basic functios, the rest is left
to the user to implement it.

* *hera_init* inits a simplified version of the augeas core that mainly loads 
lens modules. 
* *hera_get* parses a string in form of char pointer and returns a tree.
* *hera_put* put parses a tree and returns a char pointer built from values.
* *hera_close* function frees the loaded modules and other core stuff.


Lenses
------
It comes with all the lens files of the augeas project but they are installed
in another directory so they don't collide.

How to install
--------------
To build the project you have to run *./configure* and *make* and *make install*
as root user. More information in the INSTALL document.

If you are building from the repository you have to run *./autogen.sh* first and
then *make* and *make install*. It requires to have autoconf, automake, libtool and
git installed.

