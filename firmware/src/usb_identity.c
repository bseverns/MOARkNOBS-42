#include "usb_names.h"

#define MN42_MANUFACTURER_NAME {'B', 'S', 'S', 'S', 0, 0, 0, 0, 0, 0, 0}
#define MN42_MANUFACTURER_NAME_LEN 4

#define MN42_PRODUCT_NAME {'M', 'N', '4', '2', ' ', 'M', 'I', 'D', 'I', 0, 0}
#define MN42_PRODUCT_NAME_LEN 9

struct usb_string_descriptor_struct usb_string_manufacturer_name = {
    2 + MN42_MANUFACTURER_NAME_LEN * 2, 3, MN42_MANUFACTURER_NAME};

struct usb_string_descriptor_struct usb_string_product_name = {2 + MN42_PRODUCT_NAME_LEN * 2, 3,
                                                               MN42_PRODUCT_NAME};
