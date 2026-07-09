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


def esc(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def main():
    body = read_body()
    print("Content-Type: text/html")
    print()
    print("<html><body>")
    print("<h1>CGI multipart echo</h1>")
    print("<ul>")
    for key in ["REQUEST_METHOD", "CONTENT_TYPE", "CONTENT_LENGTH", "QUERY_STRING"]:
        print("<li>{0}={1}</li>".format(key, esc(os.environ.get(key, ""))))
    print("</ul>")
    print("<pre>{0}</pre>".format(esc(body)))
    print("</body></html>")


if __name__ == "__main__":
    main()
