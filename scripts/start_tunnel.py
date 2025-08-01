#!/usr/bin/env python3
import subprocess
import re
import sys
import time
import os

PAGE_DIR = "/home/ahsan/git/fin/page/"
PORT = 8000


def extract_tunnel_url(output_line):
    match = re.search(r"https://[a-zA-Z0-9\-]+\.trycloudflare\.com", output_line)
    if match:
        return match.group(0)
    return None


def main():
    os.chdir(PAGE_DIR)

    print(f"[http] starting npm server in {PAGE_DIR} on port {PORT}", file=sys.stderr)
    npm_process = subprocess.Popen(
        ["npm", "start"],
        cwd=PAGE_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    time.sleep(5)

    print(f"[tunnel] starting cloudflare tunnel for port {PORT}", file=sys.stderr)
    tunnel_process = subprocess.Popen(
        ["cloudflared", "tunnel", "--url", f"http://localhost:{PORT}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    try:
        for line in tunnel_process.stdout:  # type: ignore
            url = extract_tunnel_url(line)
            if url:
                print(url, flush=True)
                break
        tunnel_process.wait()
    except KeyboardInterrupt:
        print("[exit] terminating processes...", file=sys.stderr)
    finally:
        tunnel_process.terminate()
        npm_process.terminate()
        tunnel_process.wait()
        npm_process.wait()
        print("[exit] both npm and tunnel processes terminated", file=sys.stderr)


if __name__ == "__main__":
    main()
