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
- [ ] Add Ctrl+A, Ctrl+E, Alt+A, Alt+E
- [ ] Enable find functionality for the editor
- [ ] Add mouse capability to the editor
- [ ] Unify some interfaces
- [ ] Port all configuration to config.h file


# Planned Features
- `+<ROW>:<COL>` flag to have editor open a specific row, column of the provided text file.
- Syntax Highlighting using predefined keyword matching by baking the keywords file at compile time into the program (Or it can also have the file load at file opening time).
- Command bar using CTRL+P
- A very basic configuration file containing (config.h)
  - How many spaces should each tab use (DEFAULT: 4)
  - Colorschemes to use: Default Terminal Colors
- DEL, BACKSPACE, CTRL+DEL, CTRL+BACKSPACE and CTRL+ARROW keys should be handled (Readline like functionality)
- Simple and Minimal dialog boxes for handling basic operations
- A basic find feature
- A find and replace feature
- Enable line and word selection features
- Keymaps for CTRL+C, CTRL+V (Clipboard), CTRL+X
- Syntax Highlighting

## Keymaps
1. `CTRL + SHIFT + E`: Goto the end of the file
2. `CTRL + SHIFT + T`: Goto the top of the file
3. `CTRL + T`: Goto the top of of the line
4. `CTRL + E`: Goto the end of the line
5. `CTRL+C, CTRL+V, CTRL+X, CTRL+K`: Copy, Paste, Cut, Select-Line
6. `CTRL + HOME | END`: Goto the top/end of of the line 


# Resources 
## LOGO
▄▄▄ ▄ ▄ ▄   ▄

## Homepage
▄▄▄ ▄ ▄ ▄   ▄

be — a basic editor   v0.1
quick edits, no hassle.

─── functions & keymaps ───
Ctrl+S          save file
Ctrl+Q          quit — asks before losing changes
Ctrl+P          command palette · every function below
Ctrl+F          find in file
Ctrl+G          go to line
Ctrl+O          open a file · one buffer, one file
Ctrl+K          select line
Ctrl+T/E        goto the top or end of the line
Ctrl+Home/End   goto the top or end of the file
Ctrl+Shift+T/E  goto the top or end of the file
↑↓←→     move · Home / End · PgUp / PgDn
press Enter to open an empty buffer — or Ctrl+P for commands
