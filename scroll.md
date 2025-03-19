# The scrolling problem:
If the text is wrapped on whitespace this means that when we scroll up we do not go
backwards by text width.  It is important to update the text correctly because we want
the complete text to always look the same so that when the user can see what they expect
to see (in this case the previous text).

# 1. Find based on skipped characters
New position could be 
`InputPos - (TextWidth - Skipped)`

This means that the place where the wrapping is done must somehow return the number of
skipped characters.

Or we can run the wrapping algorithm on the current line to check if wrapping would
have been done and how much characters it would have skipped.

# 2. Wrapping as is:
Searching the text backwards for whitespace works good for an existsing text.  But we
have a text that we append to a lot, so it might be more interesting to add wrapping
when adding characters.

When we add a character at the end of the line and it is not a space *wrap*.

Problem: What if we are in the middle of the text.
1. Check the last character
2. Is it a space
    y) ignore
    n) search backwards for space and wrap there

Problem: How to print
Have the wrapping algorithms on adding a character and printing be the same, which
means that if we wrapped after printing a character we should print it correctly.

# 3. Wrapping with special characters:
To create special characters (formatting, wrapping, etc.) and preserving unicode we can
implement special characters by making our text into a special linked list structure.

> Example
```c
typedef enum {
    NodeText = 0,
    NodeBold,
    NodeWrap
} node_type;

typedef stuct {
    node_type Type;
    wchar_t *Text;
    text_node *Prev;
    text_node *Next;
} text_node;

text_node First = { NodeText, L"Hello Foo!", 0, 0 };
text_node Second = { NodeWrap, 0, &NodeText, 0 };
First.Next = &Second;
```
# Notes
- We should add a special (colored?) char to show the text is wrapped because that can
  be hard to tell when spaces are same as empty.
- What if we end up on a wrapping char?
- Implement 2 for wrapping
- Implement 1 for cursor movement
- Implement 3 for formatting later and instead use text start and finish
