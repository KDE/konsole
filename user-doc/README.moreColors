[README.moreColors]

The konsole adopted some ESC codes allowing to use extended
color spaces.

There is a predefined 256 color space compatible with his
xterm sister, and, even beyond that, a 3-byte RGB color space.

The ESC codes are as follows:

   ESC[ ... 38;2;<r>;<g>;<b> ... m   Select RGB foreground color
   ESC[ ... 48;2;<r>;<g>;<b> ... m   Select RGB background color

   ESC[ ... 38;5;<i> ... m       Select indexed foreground color
   ESC[ ... 48;5;<i> ... m       Select indexed background color

<r>,<g> and <b> are each values in the range of 0..255 and
represent the brightness as usual for the respective color.

<i> likely is a value in 0..256, but represents an indexed
color assignment composed as follows:

  0 ..  15 - System color, these are taken from the schema.
 16 .. 231 - Forms a 6x6x6 RGB color cube.
232 .. 255 - A gray scale ramp without black and white.

Try the tests/color-spaces.pl to visualize the assignment.


----------------------------------------------------------------

A note on conformance

These ESC codes break the "associativity" of SGR, but after some
research the result is, that the standard itself is broken here.

The perhaps best codes due to ECMA-48 would probably be e.g.
38:2:<r>:<g>:<b>, i.e. consistently a colon instead of a semicolon.
But apparently, this is defined different in ISO-8613 (which is
included at this place in ECMA-48), actually breaking ECMA-48.

We cannot help this and implemented the codes as above, which
is a balanced decision.

For 256 color mode, this is compatible with xterm and perhaps
with other xterm compatible terminal emulations.


| ------- Additional Comments From awendt putergeek com  2006-06-07 07:40 -------
| > So a parameter substring is 0-9 and the colon. The semicolon separates
| > sub-parameters. Thus 48:5:<Color> would be one sub-parameter, and
| > 48;5;<Color> many independent, each having an independent meaning in case
| > of a selective parameter.
| 
| 
| I think you may be onto something here with the colons... I was able to find 
| ITU T.416 (which is the same as ISO 8613-6) and it says:
| 
| --- snip ---
| 
| The parameter values 38 and 48 are followed by a parameter substring used to 
| select either the character foreground ?colour value? or the character 
| background ?colour value?.
| 
| A parameter substring for values 38 or 48 may be divided by one or more 
| separators (03/10) into parameter elements, denoted as Pe. The format of such 
| a parameter sub-string is indicated as:
| 
|          Pe : P ...
| 
| Each parameter element consists of zero, one or more bit combinations from 
| 03/00 to 03/09, representing the digits 0 to 9. An empty parameter element 
| represents a default value for this parameter element. Empty parameter 
| elements at the end of the parameter substring need not be included.
| 
| The first parameter element indicates a choice between:
| 
|            0   implementation defined (only applicable for the character 
| foreground colour)
|            1   transparent;
|            2   direct colour in RGB space;
|            3   direct colour in CMY space;
|            4   direct colour in CMYK space;
|            5   indexed colour.
| 
| If the first parameter has the value 0 or 1, there are no additional parameter 
| elements.
| 
| If the first parameter element has the value 5, then there is a second 
| parameter element specifying the index into the colour table given by the 
| attribute ?content colour table? applying to the object with which the 
| content is associated.
| 
| --- snip ---
| 
| The separator character 03/10 they use is a colon, not a semicolon... I wonder 
| if the xterm implementation was based on an improper reading of the standard?
| 
