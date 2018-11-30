import time
import logging
import signal
import datetime
import numpy as np
import matplotlib.pyplot as plt
from shlex import shlex
from subprocess import Popen, PIPE
from threading import Thread, Event, Lock
from collections import deque


log = logging.getLogger('mkrecv.monitor')

DELIMETER = None
FORMATS = {
    "STAT":[("slot-size", int), ("heaps-completed", int),
            ("heaps-discarded", int), ("heaps-needed", int),
            ("payload-expected", int), ("payload-received", int)]
}

class MkrecvMonitor(Thread):
    def __init__(self, stdout):
        """
        @brief   Wrapper for parsing the stdout from MKRECV processes.

        @param   stdout   Stdout pipe from an MKRECV process.

        @detail  MkrecvMonitor runs as a background thread that blocks on the MKRECV
                 processes stdout pipe
        """
        Thread.__init__(self)
        self.setDaemon(True)
        self._stdout = stdout
        self._params = {}
        self._watchers = set()
        self._last_update = time.time()
        self._stop_event = Event()

    def add_watcher(self, watcher):
        """
        @brief   Add watcher to the montior

        @param   watcher   A callable to be invoked on a notify_watchers call

        @detail  The watcher should take one argument in the form of
                 a dictionary of parameters
        """
        log.debug("Adding watcher: {}".format(watcher))
        self._watchers.add(watcher)

    def remove_watcher(self, watcher):
        """
        @brief   Remove a watcher from the montior

        @param   watcher   A callable to be invoked on a notify_watchers call
        """
        log.debug("Removing watcher: {}".format(watcher))
        self._watchers.remove(watcher)

    def notify_watchers(self):
        """
        @brief      Notifies all watchers of any updates.
        """
        log.debug("Notifying watchers")
        for watcher in self._watchers:
            watcher(self._params)

    def parse_line(self, line):
        """
        @brief      Parse a line from an MKRECV processes stdout

        @param      line  The line
        """
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
            self.notify_watchers()
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
        """
        @brief      Stop the thread
        """
        self._stop_event.set()

    def is_blocked(self, idle_time = 2.0):
        """
        @brief      Check if there is a prolonged block on the stdout read

        @param      idle_time  The maximum time since an update after which blocking is assumed

        @return     True if blocked, False otherwise.
        """
        return time.time() - self._last_update > idle_time


class StatisticsPlotter(object):
    def __init__(self, monitor, value_generator, title=None, ylabel="N", nvalues=100):
        self._lock = Lock()
        self._monitor = monitor
        self._value_generator = value_generator
        self._value_buffer = deque(maxlen=nvalues)
        self._timestamp_buffer = deque(maxlen=nvalues)
        self._fig = plt.figure()
        self._ax = self._fig.add_subplot(111)
        self._ax.set_title(title)
        self._ax.set_xlabel("Time")
        self._ax.set_ylabel(ylabel)
        self._last_data = None
        self._monitor.add_watcher(self.update_ringbuffers)

    def update_plot(self):
        if self._last_data:
            self._last_data[0].remove()
        with self._lock:
            self._last_data = self._ax.plot(self._timestamp_buffer, self._value_buffer, c="b")
            self._ax.relim()
            self._ax.autoscale_view()
        plt.pause(0.05)

    def update_ringbuffers(self, params):
        try:
            value = self._value_generator(params)
        except Exception as error:
            log.exception("Unable to generate value")
            return
        with self._lock:
            self._value_buffer.append(value)
            self._timestamp_buffer.append(datetime.datetime.now())


def main(mkrecv_cmdline):
    process = Popen(mkrecv_cmdline, stdout=PIPE, stderr=PIPE, close_fds=True, shell=True)
    def handler(signum, frame):
        log.info("Received signal {}, terminating processs".format(signum))
        process.terminate()
    signal.signal(signal.SIGTERM, handler)
    signal.signal(signal.SIGINT, handler)
    monitor = MkrecvMonitor(process.stdout)
    monitor.start()
    plotters = []
    plotters.append(StatisticsPlotter(monitor, lambda params: params["payload-received"] / float(params["payload-expected"]),
       title="Fraction of payload received", ylabel="Fraction", nvalues=100))
    plotters.append(StatisticsPlotter(monitor, lambda params: params["heaps-needed"],
       title="Heaps still needed", ylabel="Count", nvalues=100))
    while process.poll() is None:
        time.sleep(1)
        for plotter in plotters:
            plotter.update_plot()

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


