# fit2xmp
fit2xmp sould be a simple utility to hopefully make it easier to geotag photos you have taken while using an external gps device.

I occaisionally take photos outside, usually with a GPS device running and recording to a FIT file as a way of getting a geotag for each shot, but I never bothered combining the two until now. Since effectively all photos of mine go through adobe at some point, they should all have xmp files generated for them. This utility just goes through and matches photo time to a location in the fit file and writes it out (poorly) to the fit file.

## Warning
This was written in a weekend and is tested on a desktop by someone who does not care about memory leaks. It is used on Win 8.1 and Slackware 14.1, and not tested anywhere else. Although I included a VS solution, I left out the FIT SDK, which you can grab here: https://www.thisisant.com/resources/fit

## License
Everything in this repo not covered by the FIT SDK is under public domain.

## TODO
Obviously this is a pretty shit util in it's current state, but eventually it might get better
* Directly write the time-stamp to .NEF file format or whatever RAW format you use
* Remove the memory leaks
* Add tests
* Optimize

