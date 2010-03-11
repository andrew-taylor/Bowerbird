import threading
import time

class TokenBucket(object):
    """An implementation of the token bucket algorithm.
    source: http://code.activestate.com/recipes/511490/

    >>> bucket = TokenBucket(80, 0.5)
    >>> print bucket.consume(10)
    True
    >>> print bucket.consume(90)
    False
    """
    def __init__(self, capacity, fill_rate, lock=None):
        """capacity is the maximum tokens in the bucket.
        fill_rate is the rate in tokens/second that the bucket will be refilled.
        A negative fill_rate means that the bucket is always full.
        lock is the synchronisation lock to use (a threading.RLock if None)"""
        self.capacity = float(capacity)
        self._tokens = float(capacity)
        self.fill_rate = float(fill_rate)
        self._timestamp = time.time()
        if lock:
            self._lock = lock
        else:
            self._lock = threading.RLock()

    @property
    def tokens(self):
        with self._lock:
            self._update_tokens()
            return self._tokens

    def consume(self, tokens):
        """Consume tokens from the bucket. Returns 0 if there were
        sufficient tokens, otherwise the expected time until enough
        tokens become available."""
        if tokens > self.capacity:
            raise ValueError('Requested tokens (%f) exceeds capacity (%f)' %
                    (tokens, self.capacity))
        with self._lock:
            self._update_tokens()
            if tokens <= self._tokens:
                self._tokens -= tokens
                return 0
            return (tokens - self._tokens) / self.fill_rate

    def _update_tokens(self):
        if self._tokens < self.capacity:
            if self.fill_rate < 0:
                self._tokens = self.capacity
            else:
                now = time.time()
                # limit tokens to capacity
                self._tokens = min(self.capacity, self._tokens +
                        self.fill_rate * (now - self._timestamp))
                self._timestamp = now


