#include <Arduino.h>
#include "usb_names.h"

#if defined(ARDUINO) && defined(TEENSYDUINO)

#define MN42_MANUFACTURER_NAME {'M', 'N', '4', '2'}
#define MN42_MANUFACTURER_NAME_LEN 4

#define MN42_PRODUCT_NAME {'M', 'N', '4', '2', ' ', 'M', 'I', 'D', 'I'}
#define MN42_PRODUCT_NAME_LEN 9

extern "C" {

PROGMEM struct usb_string_descriptor_struct usb_string_manufacturer_name = {
    2 + MN42_MANUFACTURER_NAME_LEN * 2, 3, MN42_MANUFACTURER_NAME};

PROGMEM struct usb_string_descriptor_struct usb_string_product_name = {
    2 + MN42_PRODUCT_NAME_LEN * 2, 3, MN42_PRODUCT_NAME};

} // extern "C"

#endif
