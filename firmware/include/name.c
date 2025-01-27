#include "usb_names.h"

#define MIDI_NAME   {'M','O','A','R','k','N','O', 'B', 'Z'}
#define MIDI_NAME_LEN  9

// Do not change this part.  This exact format is required by USB.

struct usb_string_descriptor_struct usb_string_product_name = {
        2 + MIDI_NAME_LEN * 2,
        3,
        MIDI_NAME
};