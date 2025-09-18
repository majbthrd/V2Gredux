V2Gredux
========

This shares code that I wrote to emulate a ISO 15118-2 EVSE "charger", and I have successfully used it to communicate with an EV.

It leverages [OpenV2G](https://sourceforge.net/projects/openv2g/), which is hosted as a SVN repository on sourceforge.  I'm not aware of a way to add that repository as a git submodule, and there is no up-to-date github mirror of that project.  So, a would-be user would need to download that code into an OpenV2G subdirectory.

## Usage

Compile and run with the desired network interface provided as the first command-line argument.

The code attempts an implementation of SDP and ISO 15118-2, but does not implement the HomePlug GP SLAC negotiation.

