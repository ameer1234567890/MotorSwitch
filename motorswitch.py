#!/usr/bin/env python3
# *-* coding: utf-8 -*-
"""Check motor state periodically, ping on change, and change state on request"""

import os
import time
import requests
import tempfile
import broadlink
import multiprocessing
from http.server import BaseHTTPRequestHandler, HTTPServer

"""
Write below to config.py replacing YOUR_IFTTT_KEY_HERE as required
class Config:
    IFTTT_KEY = "YOUR_IFTTT_KEY_HERE"
"""
exec(open('./config.py').read())

CHECK_FREQ = 2  # check every 2 seconds
PID_FILE = tempfile.gettempdir() + '/motorswitch.pid'
HTTP_PORT = 8989
IFTTT_ERROR_URL = 'http://maker.ifttt.com/trigger/motor_switch_error/with/key/' + Config.IFTTT_KEY  # noqa: F821
SIGNALON_URL = 'http://motorswitch.lan/signalon'
SIGNALOFF_URL = 'http://motorswitch.lan/signaloff'
DEVICE_INDEX = 0

for _i in range(10):
    print('Discovering devices...')
    devices = broadlink.discover(timeout=5)
    print('Discovered devices: ', devices)
    print('Requesting authorization...')
    auth = devices[DEVICE_INDEX].auth()
    print('Authorization status: ', auth)
    if (auth is True):
        break
if auth is not True:
    last_error = 'Unable to get device auth!'
    print('Error: ', last_error)
    print(requests.get(IFTTT_ERROR_URL + '?value1=Error:+' + str(last_error).replace(' ', '+').replace('\n', '+')).text)


class S(BaseHTTPRequestHandler):
    def do_GET(self):  # noqa: N802
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write('<a href="rebootprocess">rebootprocess</a><br><a href="turnon">turnon</a><br><a href="turnoff">turnoff</a>'.encode('utf-8'))
        elif self.path == '/rebootprocess':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write('rebooting process'.encode('utf-8'))
            global check_thread
            check_thread.terminate()
            os.remove(PID_FILE)
            print('Restarting check process...')
            check_thread = multiprocessing.Process(target=check)
            check_thread.start()
        elif self.path == '/turnon':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write('Turning motor on'.encode('utf-8'))
            print('Turning motor on...')
            r = devices[0].set_power(True)
            print(str(r))
        elif self.path == '/turnoff':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write('Turning motor off'.encode('utf-8'))
            print('Turning motor off...')
            r = devices[0].set_power(False)
            print(str(r))
        else:
            self.send_response(404)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            self.wfile.write('Page not found'.encode('utf-8'))


def run_server(server_class=HTTPServer, handler_class=S, port=HTTP_PORT):
    server_address = ('', port)
    global httpd
    httpd = server_class(server_address, handler_class)
    print('Starting httpd...')
    httpd.serve_forever()


def check():
    print('Starting to monitor...')
    with open(PID_FILE, 'w') as fh:
        fh.write(str(os.getpid()))
        state_prev = ''
        error_count = 0
        error_notified = False
    while True:
        state_now = devices[0].check_power()
        if state_now is True or state_now is False:
            if state_now is True and state_prev is not True:
                try:
                    r = requests.get(SIGNALON_URL)
                    r.raise_for_status()
                except requests.exceptions.RequestException as e:
                    print(e)
                    last_error = e
                    error_count += 1
                else:
                    print(r.text)
                    state_prev = state_now
                    error_count = 0
                    error_notified = False
            if state_now is False and state_prev is not False:
                try:
                    r = requests.get(SIGNALOFF_URL)
                    r.raise_for_status()
                except requests.exceptions.RequestException as e:
                    print(e)
                    last_error = e
                    error_count += 1
                else:
                    print(r.text)
                    state_prev = state_now
                    error_count = 0
                    error_notified = False
        else:
            last_error = 'something went wrong. state_now = ' + str(state_now)
            error_count += 1
            print('Error: ', last_error)
        if error_count > 10 and error_notified is False:
            print(requests.get(IFTTT_ERROR_URL + '?value1=Error:+' + str(last_error).replace(' ', '+').replace('\n', '+')).text)
            error_notified = True
        time.sleep(CHECK_FREQ)


if __name__ == '__main__':
    try:
        global check_thread
        check_thread = multiprocessing.Process(target=check)
        check_thread.start()
        server_thread = multiprocessing.Process(target=run_server)
        server_thread.start()
    except KeyboardInterrupt:
        check_thread.terminate()
        httpd.server_close()
        server_thread.terminate()
        os.remove(PID_FILE)
        print('PID file removed!')
