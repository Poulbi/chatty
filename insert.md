# 1. InputBox

InputBox(u32 BoxX, u32 BoxY, u32 BoxWidth, u32 BoxHeight,
              wchar_t *Text, u32 TextLen, u32 TextIndex,
              Bool Focused)

TextIndex is where the edit cursor is in the text, so where the text should be displayed?

It is most user friendly when the text moves as least as possible, so giving an offset will make it
scroll each time the user moves the cursor.  No it should only move when the user moves the cursor
out of bounds.


There should be cursor logic and text logic

# 2. Redraw text based on cursor

```c
void
Redraw(u32 DrawnStart, u32 DrawnEnd)
{

}

Box(BoxX, BoxY, Width, Height);
BoxedText(X+1, Y+1, Width-2, Height-2,
          TextLen, Text);

if (ev.key == TB_KEY_ARROW_LEFT)
{
    CursorX--;
    TextIndex--;
}
else if (ev.key == TB_KEY_ARROW_RIGHT)
{
    CursorX++;
    TextIndex++;
}
else if (ev.key == TB_KEY_ARROW_UP)
{
    // Scroll Up, Update TextIndex, Redraw
}
else if (ev.key == TB_KEY_ARROW_DOWN)
{
    // Scroll Down, Update TextIndex, Redraw
}
else if (ev.ch)
{
    Text[Textlen++] = ev.ch;
    TextIndex++;
    CursorX++;
}

if (CursorX == BoxX + Width)
{
    // Scroll Down || Next Line (if empty)
    // Update TextIndex, Redraw
}
else if (CursorX == BoxX)
{
    // Scroll up, Update TextIndex, Redraw
}
````
It can be deterministic how we draw the text, and the cursor position is only needed to know where
to insert text.

1. Do text insertion
2. Redrawing
