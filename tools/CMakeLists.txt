
### Line graphics font
###  Attempting to auto-create LineFont.h for multiple systems is a headache.
###  If LineFont.h is needed to be recreated use:
###     fontembedder LineFont.src > LineFont.h
###  Then commit the new LineFont.h

find_package(Qt5Core ${QT_MIN_VERSION} CONFIG REQUIRED)

### Font Embedder
set(fontembedder_SRCS fontembedder.cpp)
add_executable(fontembedder ${fontembedder_SRCS})
qt5_use_modules(fontembedder Core)

