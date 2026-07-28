# termui - terminal UI using Curses

Supports configurations with preferences, character and string
input, scrolling terminal output, and help screen.

## Initialization

`Terminal_ui tu;` - create and initialize the interface 

## Top and Bottom Status Strings

`tu.fixed_info(top_lines, bottom_lines);` - reserves lines at the
    top and bottom of the screen for information.  Parameters are
    `vector<string> *` and the vectors must not be freed by the 
    caller before `tu.finish()` is called.  The vectors are reused
    for refresh and may be changed in length (number of lines) and
    content. Alternatively, this method can be called again to
    update the top and bottom text. You can also write status info
    into fixed areas using `tu.place_string()` (see below), but
    the status info will not be retained if the screen is refreshed,
    e.g., by resizing or entering a dialog. Fixed info is delineated
    from scrolling output on the screen with horizontal lines, which
    take an extra line, but no line is drawn if the vector length is
    zero or if the parameter is nullptr.
    
`tu.place_string(x, y, w, string);` - write string at x, y, padding
    to a width of w.

## Dialog

Dialogs enter a mode where the user can navigate with TAB, RETURN,
LEFT, RIGHT, UP and DOWN, and enter data into fields or press "buttons".

Dialog elements are accessed by string key.

Dialog elements are ordered in the order they are defined, so the
TAB key will move to the next element in that order.

`tu.begin_dialog(fnptr);` - prepares to create a new dialog. Call fnptr
    when a field is modified as follows: `fnptr(key, value_string);`
    The value_string may not be a valid representation.

`tu.field_int(label, key, min, max, x, y, w1, w2);` - put a labeled
    integer field on the screen with min and max values.
    
`tu.field_double(label, key, min, max, x, y, w1, w2);` - put a labeled
    double field on the screen with min and max values.
    
`tu.field_ip(label, key, x, y, w1);` - put a labeled IP address
    field on the screen displayed like ___.___.___.___
    
`tu.field_string(label, key, x, y, w1, w2);` - put a labeled 
    string field on the screen.
    
`tu.field_bool(label, key, x, y, w1, w2);` - put a labeled 
    Boolean field on the screen.

`tu.field_button(label, key, x, y, w1, w2);` - put a labeled 
    button field on the screen. Typing one of xXyXtT causes the
    callback to be called with key and an empty string.

`tu.field_menu(label, key, &value, x, y, w1, w2, options);` - put a
    labeled menu on the screen. Options are an array of string
    pointers ending in NULL, and the value stored is the integer
    index of the option. Up/Down arrows scroll through selections.
    
`tu.set_menu_options(key, options, n);` - you can dynamically change the 
    list of options by calling this method. After the change, the 
    current selection is set to n, which must index one of the options. 
    
`tu.dialog_escape_keys(keys);` - gives a string of characters that
    can be used to escape the current dialog. Default is "", and
    there should be buttons in the dialog to "Confirm", "Cancel",
    "Quit", etc.  This method should be called *each* time a dialog
    is created.
    
`tu.set_field_callback(fnptr);` -  The callback fnptr is called whenever
    the field is modified. The function call looks like fnptr(key, content)
    where key and content are strings. The content may not be a valid
    value for this field since it is being edited. This method should be
    called *each* time a dialog is created. It can be called anytime after
    `begin_dialog()`. When the dialog exits, the key used to escape is
    passed using `fnptr("END_DIALOG", string(1, static_cast<char>(key)));`
    
`tu.end_dialog(int key);` - exits the `run_dialog()` method. Call this from
    a field_callback function to cause the dialog processing to end. The key
    is passed to the `field_callback` function (see above).
        
`tu.run_dialog();` - runs the dialog as specified.
    
Any field can be initialized or changed with:

`tu.set_field(key, content);` - where content is a string. An empty
    string indicates a default value, which is the initial value unless
    `set_field` changes it.
    
You can manipulate the dialog:

`tu.field_delete(key);` - remove the field with the key

`tu.dialog_insert_line(n);` - open up a blank line in the dialog by moving 
    all fields with y >= n to y+1.
    
`tu.dialog_remove_line(n);` - close up a blank line in the dialog by moving 
    all fields with y > n to y-1. It is an error if any field has y == n.
    

Afer run_dialog() returns, you can retrieve values as follows:

`bool tu.has_value(key);` - returns true if the field is non-empty.

`int tu.get_int(key, default);` - get final integer value or default 
    if the field is blank. 

`double tu.get_double(key, default);` - get final double value or default 
    if the field is blank. 

`bool tu.get_bool(key, default);` - get final Boolean value or default 
    if the field is blank. 

`string tu.get_string(key, default);` - get final string value or default 
    if the field is blank. 

`string tu.get_ip(key, default);` - get final string value or default 
    if the field is blank. 

`int tu.get_menu(key);` - get final integer value.


## Configuration

Configuration is done in a control panel with fields and values.
Multiple configurations are stored in a preference file and the
user can select among named configurations as well as create new
configurations and edit old ones.

Configuration builds upon the Dialog interface shown above.

`config.h` reads and writes JSON, but only supports a dictionary
of configurations, where each configuration is a list of
objects, where objects have one key and values that are either
strings or lists of strings. The "__configuration__" configuration
is special: it just names the current or default initial
configuration to use. Here is an example file:

    {
      "__configuration__": [
        {"__configuration__": "DevelopementProfile"}
      ],
      "DevelopmentProfile": [
        {"BannerText": "Debug Mode Active\nLine Two"},
        {"AllowedIPs": ["127.0.0.1", "192.168.1.50"]}
      ],
      "ProductionProfile": [
        {"BannerText": "Live Environment\nLine Two"},
        {"AllowedIPs": ["10.0.0.1"]},
        {"AllowedIPs": ["172.16.0.1"]}
      ]
    }


# Dialog to Configuration

It is up to application code to move between dialog fields and
configurations. They share the same keys, but not all dialog
fields represent configuration items, and some configuration
items include data from multiple fields.

For Arco server, we can have multiple configuration items
representing `O2_to_OSC`, so these all have the same key. In
the dialog fields, we distinguish these by line number, and
in the configuration file, data on the same line are packed
into a string list for the same object.

Example: The interface has:

    MIDI Out Service a______ to port1_______ (X_)
    MIDI Out Service b______ to port2_______ (X_)

So the fields contain keys `MIDI_out_service` and `MIDI_out_port`.
There will be 2 of each, but they will be in different lines (y).
The configuration file, these will be:

    {
      "Configuration1": [
        {"MIDI_out": ["a", "port1"]}, 
        {"MIDI_out": ["b", "port2"]}
      ]
    }

### Interface:

Configuration files contain attribute/value pairs in multiple
named "dicationaries". These are represented in memory as a
map` mapping from configuration names to maps`
mapping keys to string values. To transfer data from a dialog to
a configuration, each field in the dialog has a flag to indicate
that it should be stored in the configuration. Configuration data
can also be derived, so when the configuration is read or written,
the application can make additional calls to read/write configuration
data and dialog fields.



In these calls, key should be the string name of the attribute used in the
configuration file. It also serves as the key (name) for the field.

`tu.config_int(label, key, x, y, w1, w2);` - put a labeled integer field 
    in the configuration screen. 

`tu.config_double(label, id, key, min, max, x, y, w1, w2);` - put a labeled
    double field in the configuration screen. 

`tu.config_int(label, id, key, min, max, x, y, w1, w2);` - put a labeled
    integer field in the configuration screen at location (x, y), with
    width w1 for the label and w2 for the integer field. The integer
    must be between min and max, inclusive. key is the name of the field
    in the configuration file. 

`tu.config_ip(label, id, key, x, y, w1);` - put a labeled
    ip address in the configuration screen at location (x, y), with
    width w1 for the label. 

`tu.config_string(label, id, key, x, y, w1, w2);` - similar to config_int.

`tu.config_bool(label, id, key, x, y, w1, w2);` - similar to config_int.
    Bools are displayed as T or F. w2 is normally 1.
    
`tu.config_button(label, id, x, y, w1, w2);` - similar to config_int,
    but typing one of xXyYtT into the field triggers an action.
    
`tu.config_menu(label, id, key, x, y, w1, w2, options);` - similar to
    config_int, but up/down arrows select among options.  Options are 
    an array of string pointers ending in NULL, and the value stored 
    is the integer index of the option.
    
`tu.set_menu_options(id, options, n);` - you can dynamically change the 
    list of options by calling this method. After the change, the 
    current selection is set to n, which must index one of the options. 
    
# Help

`tu.add_help(help_string);` - add a help string

`tu.help_keys(keys);` - give a list of characters that cause help to
    be printed.

# Interaction

`tu.poll();` - checks for input and updates display.

`tu.set_key_callback(callbackfn);` - call to callbackfn when a key is
    pressed but not processed by Terminal_io. The function is called as:
    `callbackfn(int key, bool is_escape)` where `is_escape` indicates
    that this is the character that was typed to exit from run_dialog()
    or configure().
    


