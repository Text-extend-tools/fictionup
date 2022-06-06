FICTIONUP MARKDOWN EXAMPLE
==========================

---
Annotation: |
    The example of prepared for conversion to FB2 markdown file.

    Let's make this annotation more *fancy* by adding `markdown` to it.
Author: Vadim Druzhin
Year: 2019
Genre: computers
Cover: fictionup.png
Series: [All about FICTIONUP, 1]
fb2-author: Vadim Druzhin
fb2-date: 2019
fb2-id: fictionup-example-en-0002
fb2-version: 1.0
---

Chapter 1. Text formatting
==========================

Here goes regular text. *This text will be emphasised (italic).*
**And this text will be more emphasised (bold).**
***And this will be even more emphasised (bold italic).***
~~This text will be struck through~~ (if your FB2 viewer supports this feature).

This text will be accompanied by^(superscript (upper index\)).
`And this will be *inline* code span.` Followed by another regular text.

Chapter 2. Special paragraph types
==================================

2.1. Unordered list
-------------------

Unordered list may look like this:

* First item
* Second item
* Third item

Or like this:

- First item
- Second item
- Third item

2.2. Ordered list
-----------------

Ordered list will be like that:

1. First item
2. Second item
3. Third item

In fact, item numbers does not matter:

1. *First* item
1. **Second** item
1. ***Third*** item

2.3. Citation
-------------

Citation may be extended across several paragraphs:

> Some very important things that was said.
> By some very important person.
>
> And this *things* also **may have** different formatting.

2.4. Preforamted code
---------------------

Code block will look like this:

```
#include <stdio.h>

int main()
    {
    printf("*Hello* **world!**\n");
    }
```

2.5. Tables
-----------

Not all FB2 viewers support tables, so this text may look as a nice table or not.

| First column (left aligned) | Second column (centered) | Third column (right aligned) |
|-----------------------------|:------------------------:|-----------------------------:|
| Some                        | rows                     | of text                      |
| And                         | some                     | more                         |

Chapter 3. Delimiters
=====================

Paragraphs in document may be delimited by some sort of horizontal ruler. Like this:

* * *

Or this:

- - -

Or this:

_ _ _

Also paragraph containing just one '\\' symbol will produce empty line.

\

Like above this one.

Chapter 4. Links and images
===========================

Document may contain hyperlinks: [cdslow.org.ru](http://cdslow.org.ru/).

And images: ![](fictionup.png).

Some text may be followed by in depth explanation[^note 1].

[^note 1]: In form of footnotes.

* * *

__The End__

