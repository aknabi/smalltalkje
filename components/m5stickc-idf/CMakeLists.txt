# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly

# include paths of this components.
set(COMPONENT_ADD_INCLUDEDIRS include util)

# source files of this components.
set(COMPONENT_SRCDIRS . util util/font backported/esp_event backported/newlib)

# add this components, this will define a CMake library target.
register_component()
