import time
import logging
import signal
import numpy as np
from shlex import shlex
from subprocess import Popen, PIPE
from threading import Thread, Event

log = logging.getLogger('mkrecv.monitor')

DELIMETER = None
FORMATS = {
    "XXXXXX":[("test1", float),("test2", int)]
}

class MkrecvMonitor(Thread):
    def __init__(self, stdout):
        Thread.__init__(self)
        self.setDaemon(True)
        self._stdout = stdout
        self._params = {}
        self._watchers = set()
        self._last_update = time.time()
        self._stop_event = Event()

    def add_watcher(self, watcher):
        log.debug("Adding watcher: {}".format(watcher))
        self._watchers.add(watcher)

    def remove_watcher(self, watcher):
        log.debug("Removing watcher: {}".format(watcher))
        self._watchers.remove(watcher)

    def notify_watcher(self):
        log.debug("Notifying watchers")
        for watcher in self._watchers:
            watcher(self._params)

    def parse_line(self, line):
        line = line.strip()
        log.debug("Parsing line: '{}'".format(line))
        if len(line) == 0:
            log.warning("Zero length line detected")
            return
        split = line.split(DELIMETER)
        key = split[0]
        if key in FORMATS:
            log.debug("Using formatter for key: {}".format(key))
            formatter = FORMATS[key]
            for (name, parser), value in zip(formatter,split[1:]):
                self._params[name] = parser(value)
            log.info("Parameters: {}".format(self._params))
            self.notify_watcher()
        else:
            log.debug("Invalid key: {}".format(key))

    def run(self):
        for line in iter(self._stdout.readline, b''):
            self._last_update = time.time()
            try:
                self.parse_line(line)
            except Exception as error:
                log.exception("Unable to parse line: '{}'".format(line))
            if self._stop_event.is_set():
                break

    def stop(self):
        self._stop_event.set()

    def is_blocked(self, idle_time = 2.0):
        return time.time() - self._last_update > idle_time

def main(mkrecv_cmdline):
    process = Popen(mkrecv_cmdline, stdout=PIPE, stderr=PIPE, close_fds=True, shell=True)
    def handler(signum, frame):
        log.info("Received signal {}, terminating processs".format(signum))
        process.terminate()
    signal.signal(signal.SIGTERM, handler)
    signal.signal(signal.SIGINT, handler)
    monitor = MkrecvMonitor(process.stdout)
    monitor.start()
    process.wait()
    print process.stdout.read()
    print process.stderr.read()

if __name__ == "__main__":
    import sys
    import coloredlogs
    coloredlogs.install(
        fmt="[ %(levelname)s - %(asctime)s - %(name)s - %(filename)s:%(lineno)s] %(message)s",
        level="DEBUG",
        logger=log)
    cmd_line = " ".join(sys.argv[1:])
    log.info("Running MKRECV monitor with command line: '{}'".format(cmd_line))
    main(cmd_line)


