# OpenRoll 0.17.2 beta patches

The source in this ZIP already includes every patch. See the top-level README
to build and run it.

To replay the changes on a clean OpenRoll 0.16.0 source tree, run the following
from the root of that older tree in Bash. Replace the path with the extracted
beta folder.

```bash
for patch in /path/to/OpenRoll-0.17.2/patches/*.patch; do
    git apply --check "$patch" && git apply "$patch" || exit 1
done
```

Apply the patches in numerical order. Patches 01–07 cover the UI redesign.
Patch 08 links footage dropped into Media and Ingest to the active project.
Patch 09 simplifies Ingest and moves verified copy into the Back up dialog.
The series reconstructs the code, tests, build scripts, and user documentation.
Build logs, captures, and release checksums are shipped with the ZIP.
