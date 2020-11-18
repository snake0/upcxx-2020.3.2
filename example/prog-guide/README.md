This is code from the programmer's guide. It can be modified here, but any updated code files should
be copied to the code/ subdirectory in the upcxx-prog-guide repo, and then put into guide.md with
the put-code.py script provided in the upcxx-prog-guide repo. Code should not be modified or added
within guide.md.  

To build all code, make sure to first set the `UPCXX_INSTALL` variable. e.g. 

`export UPCXX_INSTALL=<installdir>`

then

`make all`

Otherwise, e.g.

`make hello-world`

Run as usual, e.g. 

`upcxx-run -n 4 ./hello-world`
