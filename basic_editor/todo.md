# All Tasks 
## Focus : V1.0
- [x] Draw the basic-editor welcome message in the center of the editor
    - NOTE: Opening a file opens the buffer and the welcome message is now removed (Screen Refreshes)
- [x] Enable cursor moving and basic text editing in the editor
- [x] Full word processing based navigation. CTRL+HOME, CTRL+END, PAGEUP, PAGEDOWN, HOME, END should be handled
- [x] Convert the current text viewer to a text editor
- [x] Floating window confirmation for quitting without saving the changes
- [x] Enable the status-message functionality.
- [x] Make command pallete and also have a switch theme functionality for it 
- [x] Enable Open & Goto Dialog boxes
- [x] Enable readline like editing in the editor | i.e. use a line struct
- [x] Add Ctrl+A, Ctrl+E, Alt+A, Alt+E
- [x] Ctrl+O Shows first few files/folders in directory under (Non Interactive)
- [x] Enable find functionality for the editor
- [x] Enable line editing and navigation on the dialog boxes too
- [ ] Unify some interfaces and colorschemes
- [x] Port all configuration to config.h file (Suckless Style !!)
- [x] Add mouse capability to the editor -- Only Scrolling (main editor) and Click moves the cursor and presses buttons


# Planned Features
- ~A full text editor which can load and edit files without having much learning curve.~
- ~Simple mouse mode which can be used for navigation (on by default).~
- ~Low on disc space and memory (~70kb Executable, takes about 2M memory for opening a file of 2500+ lines).~
	- vi mem usage: 14M
	- nvim mem usage:
- `+<ROW>:<COL>` flag to have editor open a specific row, column of the provided text file.
- A basic cli interface which has `--help` and `--version` flag along with the `+` flags
- Syntax Highlighting using predefined keyword matching by baking the keywords file at compile time into the program (Or it can also have the file load at file opening time).
- ~Command Palette using CTRL+P~
- A basic suckless style configuration file (config.h) for configuring various settings for the editor
- ~DEL, BACKSPACE, CTRL+DEL, CTRL+BACKSPACE and CTRL+ARROW keys should be handled (Readline like functionality)~
- ~Simple and Minimal dialog boxes for handling basic operations~
- ~A basic find feature~
- A find and replace feature
- ~Enable line and word selection features~
- Keymaps for CTRL+C, CTRL+V (Clipboard), CTRL+X
- Automatic pairing for brackets and quotes
- A very basic file explorer like interface embedded in the open dialog
- ?? Enable clean patching using `patch.h` and `patch.c` files ??

# Stats on Memory usage (Ram)
> **File:** main.c (BE V1.0 source code -- ~3300 loc)
1. BE   : ~2M
2. vi   : ~14M
3. nvim : ~15M
4. nano : ~6.3M
