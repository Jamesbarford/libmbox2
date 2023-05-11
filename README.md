# libmbox2 (WIP)
- Handle file io
- Can take a buf which is 1 mbox buffer and parse that into something meaningful
- Can query the mbox file
- Can index an mbox file
- Multi-threaded and _extrememly_ fast
    - 5gb, before indexing -> 2 seconds
    - 5gb, after indexing -> ~800ms
- Easy to use

## TODO
- Extract attachments
    - images
    - PDFS
    - ...etc (basically anything)
- Parse HTML body with libxml to give a better preview
- Delete messages & update mbox file

# Building
- Run `autogen.sh`
- Run `./configure`
- Run `make`
- Run `make install` - will install header files and static libraries

__Script:__
```sh
./autogen.sh && ./configure && make && make install
```

# Usage
Here is a command line tool that will take an mbox file as the first argument
and a search term as the second. It will parse the file and then print out a
slimmed down version of the emails, if any, that were found.

```c
#include <stdio.h>
#include <libmbox2.h>

#define THREAD_COUNT (8)
#define FILE_PERMISSIONS (0666)

int
main(int argc, char **argv)
{ 
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file> <search_term>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *file_path = argv[1];
    char *from = argv[2];

    mbox *mbox_handle = mboxReadOpen(file_path, FILE_PERMISSIONS);
    /* A doubly circular linked list of messages */
    mboxList *messages = mboxParseFile(mbox_handle, THREAD_COUNT);

    /* Case insensitive search will find messages from '<from>', this will
     * create a new linked list */
    mboxList *sender_msgs = mboxMsgListFilterBySender(messages, from);

    size_t count = sender_msgs->len;
    mboxMsgLite *msg = NULL;
    mboxListNode *n = sender_msgs->root;

    do {
        msg = n->data;
        mboxMsgLitePrint(msg);
        n = n->next;
        count--;
    } while (count > 0);

    mboxListRelease(messages);
    mboxListRelease(linkedin_msgs);
    mboxRelease(mbox_handle);
}
```
## Creating and loading from indexes
By saving all off the offsets of where messages start and end this offers
significantly faster performance for subseqent loading as we know where all of
the messages are. Meaning we don't need to scan through millions of characters
trying to find from lines.

```c
#include <stdio.h>
#include <libmbox2.h>

#define THREAD_COUNT (8)

void
save(void)
{
    char *file_path = "example.mbox";
    char *idx_file = "example-mbox-index";

    mbox *mbox_handle = mboxReadOpen(file_path, FILE_PERMISSIONS);
    /* A doubly circular linked list of messages */
    mboxList *messages = mboxParseFile(mbox_handle, THREAD_COUNT);
    
    /* Save indexes of messages in a file */
    if (mboxIdxSave(idx_file, messages) != 1) {
        fprintf(strderr, "Failed to save indexes\n");
        return 0;
    }

    mboxListRelease(messages);
    mboxRelease(mbox_handle);
}

/* Significantly easier to load */
void
load(void)
{
    char *file_path = "example.mbox";
    char *idx_file = "example-mbox-index";

    mboxList *messages = mboxIdxLoad(idx_file, file_path, THREAD_COUNT);

    if (messages == NULL) {
        fprintf(strderr, "Failed to load indexes\n");
        return 0;
    }

    mboxListRelease(messages);
}

```


__Compile:__
```sh
cc <file> -lmbox2
```

On mac you might need to specify the library path:

```sh
cc -L/usr/local/lib <file> -lmbox2
```

Then lets find messages from linkedin

```sh
./a.out ./file.mbox linkedin
```

# Why?
I built this as I ran out of storage on Gmail, and, being frugal, I refused
to buy cloud storage from them. Instead I downloaded an mystical `mbox` file which
initially I had no idea what it was for. After trying to open it in a text
editor and it crashing I was further intrigued by the `5gb` of `mbox`.
Armed with my trusty vi (because why make life easy?), I opened it, and what do
I see? Something looking suspiciously like HTTP headers with a body, kind of 
like me after a week of fast food. It took me all of 2 seconds to realize that 
going through 10 years of spam, some in base64, in vi, wasabout as fun as a
root canal. I'm many things, but a masochist isn't one of them.

So I looked for tools of which I found mailutils which looks like it's geared
towards low powered devices. I have a 16gb 2018 macbook pro with pthreads, not 
a car stereo so wanted something with a bit more **oomph**!. I then found
(libmbox)[https://sourceforge.net/p/vfsmail/code/] which
hasn't been updated since 2004 and didn't offer the functionality I required nor
speed for 5gb of files. It does however have a much simpler parser being about
~500 lines long it was also unable to retrieve the information I need out of 
the email namely PDF's, Images and read the text as ASCII.

## Why C?
It's the only _fast_ language I am half decent with, I would like to build a
python wrapper over the top of it to make it easier to use. And possibly a 
node JS one.

# Terminology
__`'From line':`__
A 'From line' is what this library calls the line beginning with 'From ' for 
example:

```
From <somevalue> <date>
```
GMail's one looks something like the following:

```
From 1754173012221210512@xxx Thu Jan 05 09:09:08 +0000 2023
```

__`'Headers'`:__
These are the headers of the email and are almost verbatim in style to HTTP 
headers.


## Thanks
- `libxml`, from reading the code I was able to come up with a better structure
  for the parser and buffer. Would recommend reading, it's very well commented
  and has many interesting ideas.
