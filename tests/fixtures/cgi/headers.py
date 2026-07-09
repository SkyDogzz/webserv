#!/usr/bin/env python3

import os
import sys


def read_body():
    try:
        length = int(os.environ.get("CONTENT_LENGTH", "0"))
    except ValueError:
        length = 0
    if length <= 0:
        return ""
    return sys.stdin.read(length)


def main():
    body = read_body()
    print("Content-Type: text/html")
    print()
    print("<html><body>")
    print("<h1>CGI header dump</h1>")
    print("<pre>")
    for key in sorted(os.environ):
        if key.startswith("HTTP_") or key in (
            "REQUEST_METHOD",
            "SCRIPT_NAME",
            "SCRIPT_FILENAME",
            "QUERY_STRING",
            "CONTENT_LENGTH",
            "CONTENT_TYPE",
        ):
            print("{0}={1}".format(key, os.environ.get(key, "")))
    print("</pre>")
    print("<pre>{}</pre>".format(body.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")))
    print("</body></html>")


if __name__ == "__main__":
    main()
