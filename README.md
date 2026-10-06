duti
====

duti is a command-line utility capable of setting default applications for
various document types on [macOS](https://www.apple.com/macos/), using Apple's
[Uniform Type
Identifiers](https://developer.apple.com/library/content/documentation/FileManagement/Conceptual/understanding_utis/understand_utis_intro/understand_utis_intro.html)
(UTI). A UTI is a unique string describing the format of a file's content. For
instance, a Microsoft Word document has a UTI of `com.microsoft.word.doc`. Using
`duti`, the user can change which application acts as the default handler for a
given UTI.


Installing
----------

With [Homebrew](https://brew.sh):

    brew install duti


Compiling
---------

Building from source needs the Xcode command line tools and autoconf
(`brew install autoconf`):

    autoreconf -i
    ./configure
    make
    sudo make install


Usage
-----

`duti` can read settings from four different sources:

1. standard input

1. a settings file

1. an XML [property list](https://en.wikipedia.org/wiki/Property_list) (plist)

1. command-line arguments.

A settings line, as read in cases 1 and 2, consists of an application's bundle
ID, a UTI, and a string describing what role the application handles for the
given UTI. The process is similar when `duti` processes a plist. If the path
given to `duti` on the command-line is a directory, `duti` will apply settings
from all valid settings files in that directory, excluding files whose names
begin with `.` (single dot).

`duti` can also print out the default application information for a given
extension (`-x`). This feature is based on public domain source code posted
by Keith Alperin on the heliumfoot.com blog.

Options
-------

| Option | Description |
|---|---|
| `-s bundle_id type [role]` | Set the default handler. `type` is a UTI, an extension (`.pdf` or `pdf`) or a MIME type (`text/html`) when a role is given, and a URL scheme (`https`) when it is not. `role` is one of `none`, `viewer`, `editor`, `shell` or `all`. |
| `-d uti` | Print the default handler for a UTI or URL scheme. |
| `-l uti` | Print all handlers for a UTI or URL scheme. |
| `-x ext` | Print the name, path and bundle ID of the default application for an extension. |
| `-e ext` | Print the UTI declarations for an extension. |
| `-u uti` | Print the declaration of a UTI. |
| `-v` | Verbose output. |
| `-V` | Print the version. |
| `-h` | Print usage. |

See the man page for additional usage details.


Examples
--------

* Set Safari as the default handler for HTML documents:

    ```sh
    duti -s com.apple.Safari public.html all
    ```

* Set TextEdit as the default handler for Word documents:

    ```sh
    echo 'com.apple.TextEdit   com.microsoft.word.doc all' | duti
    ```

* Set Finder as the default handler for ftp:// URLs:

    ```sh
    duti -s com.apple.Finder ftp
    ```

* Get default application information for .jpg files:

    ```sh
    duti -x jpg

    # Output
    Preview
    /Applications/Preview.app
    com.apple.Preview
    ```

Support
-------

`duti` is unsupported. You can submit bug reports and feature requests at
the duti [GitHub project page](https://github.com/moretension/duti/issues).

Related
-------
[dutis](https://github.com/tsonglew/dutis) is a wrapper around duti, providing an
interactive interface to select default applications.

License
-------

`duti` was originally released into the public domain by Andrew Mortensen
in 2008. It's provided as is without warranties of any kind. You can do
anything you want with it. If you incorporate some or all of the code into
another project, I'd appreciate credit for the work I've done, but that's all.

Andrew Mortensen
April 2018
