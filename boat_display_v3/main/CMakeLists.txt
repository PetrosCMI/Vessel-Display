# Get all source files in the src directory
file(GLOB_RECURSE SOURCES *.cpp *.c)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "." "include"
    # This tells 'src' that it depends on the LCD driver below
    REQUIRES esp32_s3_touch_lcd_4b 
)
