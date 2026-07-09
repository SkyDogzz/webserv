#!/usr/bin/env python3

import os
import sys
from urllib.parse import parse_qs


def main():
    query = parse_qs(os.environ.get("QUERY_STRING", ""))
    code = query.get("code", ["200"])[0]
    try:
        status = int(code)
    except ValueError:
        status = 200

    print("Status: {0} Test Status".format(status))
    print("Content-Type: text/plain")
    print()
    print("status={0}".format(status))
    print("method={0}".format(os.environ.get("REQUEST_METHOD", "")))
    print("query={0}".format(os.environ.get("QUERY_STRING", "")))


if __name__ == "__main__":
    main()
