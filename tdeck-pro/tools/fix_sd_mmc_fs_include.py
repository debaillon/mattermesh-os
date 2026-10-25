Import('env')
# ESP32-audioI2S's Audio.h unconditionally includes <SD_MMC.h>; that library's own library.properties
# declares no dependency on FS, so PlatformIO's dependency finder never adds FS's include directory to
# SD_MMC.cpp's compile command - "FS.h: No such file or directory" without this.
import os
platform=env.PioPlatform()
fw=platform.get_package_dir('framework-arduinoespressif32')
if fw:
 fs_include=os.path.join(fw,'libraries','FS','src')
 if os.path.isdir(fs_include):
  env.Append(CPPPATH=[fs_include])
