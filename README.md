# eqmod
contribution to indi eqmod driver for astronomy. Modification for skywatcher wave150i mount

indi eqmod driver is part of INDI/3rdparty drivers maintained and available here: https://github.com/indilib.

The skywatcher mount wave150i use the skywatcher protocol and it is seen by the eqmod driver.
However, the stepper motor has parameters that slighly differ from those expected by the eqmod driver:
The reference (Init) position is different for celestial RA axis from what is defined in the eqmod driver.
Consequently, goto made before syncing to a correct target may be wrong. And before syncing the mount it's nice to perform a goto to the target...
This also implies wrong goto eastern and western limits as they are defined with respect to Init stepper position.

The code available here is under testing and propose some modifications to handle wave150i mount.
The modifications are activated by the macro "EQMODE_EXT" defined in skywatcher.h

To use these codes, download the "eqmodebase.cpp skywatcher.cpp skywatcher.h" files in your indi-3rdparty/indi-eqmod directory AFTER making a copy of the original files. Recompile the driver by typing "make indi_eqmod_telescope" from "build-indi-3rdparty" directory.

