# All Tasks 
## Main Focus
- [x] Draw the basic-editor welcome message in the center of the editor
    - NOTE: Opening a file opens the buffer and the welcome message is now removed (Screen Refreshes)
- [x] Enable cursor moving and basic text editing in the editor
- [x] Full word processing based navigation. CTRL+HOME, CTRL+END, PAGEUP, PAGEDOWN, HOME, END should be handled
- [ ] Convert the current text viewer to a text editor
- [ ] Port all configuration to config.h file
- [ ] Unify some interfaces
- [ ] Make command pallete and also have a switch theme functionality for it 
- [ ] Port row also into a string builder


# Planned Features
- `+<ROW>:<COL>` flag to have editor open a specific row, column of the provided text file.
- Syntax Highlighting using predefined keyword matching by baking the keywords file at compile time into the program (Or it can also have the file load at file opening time).
- Command bar using CTRL+P
- A very basic configuration file containing (config.h)
  - How many spaces should each tab use (DEFAULT: 4)
  - Colorschemes to use: Default Terminal Colors
- Refine the cursor position logic (Calculating rx on tabs)
- DEL, BACKSPACE, CTRL+DEL, CTRL+BACKSPACE and CTRL+ARROW keys should be handled

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
