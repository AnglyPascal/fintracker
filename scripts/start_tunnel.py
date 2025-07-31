#!/usr/bin/env python3
import subprocess
import http.server
import socketserver
import threading
import re
import sys
import time
import os

PAGE_DIR = "/home/ahsan/git/fin/page/"
PORT = 8000

def start_http_server():
    handler = http.server.SimpleHTTPRequestHandler
    with socketserver.TCPServer(("", PORT), handler) as httpd:
        print(f"[HTTP] Serving directory on http://localhost:{PORT}", file=sys.stderr)
        httpd.serve_forever()

def extract_tunnel_url(output_line):
    match = re.search(r"https://[a-zA-Z0-9\-]+\.trycloudflare\.com", output_line)
    if match:
        return match.group(0)
    return None

def main():
    # Change to page directory
    os.chdir(PAGE_DIR)

    # Start HTTP server in background thread
    server_thread = threading.Thread(target=start_http_server, daemon=True)
    server_thread.start()

    # Give server time to start
    time.sleep(1)

    # Launch cloudflared process
    process = subprocess.Popen(
        ["cloudflared", "tunnel", "--url", f"http://localhost:{PORT}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1
    )

    try:
        for line in process.stdout:
            url = extract_tunnel_url(line)
            if url:
                print(url, flush=True)  # <-- This is the ONLY stdout
                break
        process.wait()
    except KeyboardInterrupt:
        process.terminate()
        print("[Main] Terminated by user.", file=sys.stderr)

if __name__ == "__main__":
    main()
