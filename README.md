This is a code dump associated with the benchmarking results that I [posted to the x264 mailing list](https://mailman.videolan.org/pipermail/x264-devel/2014-September/010800.html) in September 2014.

At the time, I documented the changes to x264 that I made in the posts, but I recently received a request for the original code.  Rather than send code to the sole requester, I'm making it universally available.  I DO NOT purport this to be ready-to-use code.

This code was compiled at the time with v3.2 of [Rowley Crossworks for ARM](http://www.rowley.co.uk/arm/) using the optional clang compiler.

As written, it leverages some of the [I/O APIs](http://www.rowleydownload.co.uk/arm/documentation/debugio_h.htm) provided as part of their IDE solution.  I also leveraged the 'Target -> Download File' menu in the IDE to make it easy to pre-program the source material video frames into an unused region of the microcontroller's flash.

