# Free Software should have FREE TOOLS

This started 7 or so years ago when I was using LVGL, there was nothing of the sort.  I started talking about doing something like this, and they created one.  Well it got spun off and another one got created.  These all cost money and annoyed me as cash grabs, so I decided to finish this.

# RFC 
If there is a bug or a feature you would like to see, please feel free to use Github Issues.  I would really like this to be the defacto editor for LVGL, and that can only happen with feedback.
Also, feel free to submit well documented pull requests so this just is not MY tool, but the community's.

# What is LVGL UI Creator?
Glad you asked.  It is a gui for creating LVGL GUIs.  It is written using LVGL so it is truly WYSIWYG.  There are quite a few features that I am proud of:
1. All LVGL Widgets and their functionality included
2. Quite a few additional widgets included, including my Advanced Keyboard that has text prediction!!!
3. Generates C code, exports MicroPython projects for the official LVGL binding, or outputs Protobuf and UI Creator XML projects for the built-in display runtimes.
4. Quite a few more items.  I am back to active development on it in my free time, so please if you want a feature or something doesnt work, open a ticket in Github and Ill get on it.  Or better yet, fork and fix as the source code is all here!

Import Figma frames using **File → Import Figma design…**. Paste an online Figma
link and provide a personal access token, or load a portable JSON file from the
included [Figma export plugin](Integrations/Figma/README.md). Both workflows embed
artwork while preserving editable text, containers, and simple shapes. Local
plugin imports require no token.

The **Advanced Widgets → Advanced Keyboard** uses the shared native keyboard in
the Creator simulator. Select its **Target text box**, then hold a character key
for one second to show accented and alternate characters. Slide onto an option
and release, or release the original key and click an option. A short click still
types the original character. Changing the language, hiding the keyboard, or
clicking outside dismisses the options. Available variants follow the selected
language and Shift state.

