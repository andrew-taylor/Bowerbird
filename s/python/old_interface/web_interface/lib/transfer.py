import os
import multiprocessing
import contextlib
import urllib
import urllib2
import copy
import time

from bowerbird.common import *
from lib.token_bucket import TokenBucket

class TransferManager(object):
    '''This class manages the transfer of files from remote bowerbird systems.
        Each transfer is executed in a separate process using a pool.
    '''
    def __init__(self, remote_port, concurrent_transfers,
            transfer_bandwidth, transfer_chunk_size):
        self._remote_port = remote_port
        self._concurrent_transfers = concurrent_transfers
        self._transfer_chunk_size = transfer_chunk_size

        self._manager = multiprocessing.Manager()
        self._tasks = self._manager.dict()
        self._lock = self._manager.RLock()
        self._pool = multiprocessing.Pool(concurrent_transfers)

        self._bandwidth_bucket = TokenBucket(transfer_chunk_size,
                transfer_bandwidth, self._manager.RLock())

        self._errors = self._manager.list()

    @property
    def queue_ids(self):
        with self._lock:
            return [recording.id for recording, address in self._tasks.values()]

    @property
    def queue_recordings(self):
        with self._lock:
            return [recording for recording, address in self._tasks.values()]

    @property
    def queue(self):
        with self._lock:
            return self._tasks.values()

    @property
    def errors(self):
        return copy.deepcopy(self._errors)

    def clear(self):
        with self._lock:
            self._pool.terminate()
            # must create new one because terminate kills the pool
            self._pool = multiprocessing.Pool(self._concurrent_transfers)
            # delete all files from recordings still in the queue because
            # those transfers are probably incomplete
            for recording in self.queue_recordings:
                if recording.fileExists():
                    os.remove(recording.abspath)
            self._tasks.clear()

    def contains(self, recording_id):
        with self._lock:
            return recording_id in self._tasks

    def add(self, recording, address):
        with self._lock:
            if not self.contains(recording.id):
                # add task to the pool
                self._pool.apply_async(_transfer, (recording, address,
                        self._remote_port, self._transfer_chunk_size,
                        self._bandwidth_bucket, self._tasks, self._errors))
                # add it to our list of current tasks
                self._tasks[recording.id] = (recording, address)

    def remove(self, recording_id):
        raise NotImplementedError()
        with self._lock:
            if self._tasks.has_key(recording_id):
                #TODO: cancel task from pool
                del self._tasks[recording_id]
            pass

    def clear_errors(self):
        while self._errors:
            self._errors.pop()


def _transfer(recording, address, remote_port, chunk_size, bandwidth_bucket,
        task_list, errors):
    try:
        url = 'http://%s:%s/recording_by_hash' % (address, remote_port)
        data = urllib.urlencode([('hash', recording.hash)])
        request = urllib2.Request(url, data)
        ensureDirectoryExists(os.path.dirname(recording.abspath))
        temp_name = recording.abspath + '.part'
        with contextlib.closing(urllib2.urlopen(request)) as remote_file:
            with open(temp_name, 'wb') as save_file:
                # copy in chunks to reduce memory usage
                chunk = remote_file.read(chunk_size)
                while chunk:
                    save_file.write(chunk)
                    # rate limit transfer
                    wait_time = bandwidth_bucket.consume(chunk_size)
                    if wait_time:
                        time.sleep(wait_time)
                    # get the next chunk
                    chunk = remote_file.read(chunk_size)

        # rename temp file to real name
        os.rename(temp_name, recording.abspath)
        # remove task from list
        del task_list[recording.id]
    except Exception, inst:
        errors.append(inst)
        print inst
